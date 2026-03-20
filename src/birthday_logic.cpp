/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Alle functies die te maken hebben met het beheren van verjaardagen, 
 * zoals het lezen en schrijven van de verjaardagslijst, het berekenen van de eerstvolgende verjaardag, 
 * en het bepalen of er een verjaardag 'binnenkort' is.
*/


#include "app_actions.h"



// Interne variabelen (alleen zichtbaar in dit bestand)
static unsigned long lastRedraw = 0;
const unsigned long REDRAW_INTERVAL = 1000; // 1 FPS is genoeg voor setup

// Schrijf de ruwe tekst uit de textarea naar LittleFS
void App::saveBirthdays(String rawText) {
    File file = LittleFS.open("/birthdays.txt", "w");
    if (!file) {
        Serial.println("Fout bij openen birthdays.txt voor schrijven");
        return;
    }
    file.print(rawText);
    file.close();
    Serial.println("Verjaardagen opgeslagen!");
}

// Haal de ruwe tekst op om weer in de textarea te tonen (WebUI)
String App::getBirthdaysRaw() {
    if (!LittleFS.exists("/birthdays.txt")) return "";
    
    File file = LittleFS.open("/birthdays.txt", "r");
    if (!file) return "";
    
    String content = file.readString();
    file.close();
    return content;
}

void App::updateDailyBirthdayState() {
    auto lijst = getSortedBirthdays(1); // Pak de eerstvolgende
    if (!lijst.empty() && lijst[0].dagenTeGaan == 0) {
        state.display.birthday_upcoming = true;
        state.display.birthday_gender = lijst[0].gender; // Sla de M of V op in de globale state
    } else {
        state.display.birthday_upcoming = false;
        state.display.birthday_gender = '?'; 
    }
}


void App::updateBirthdayList()
{
    currentBirthdays.clear();
    if (!LittleFS.exists("/birthdays.txt"))
        return;

    File file = LittleFS.open("/birthdays.txt", "r");
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    int todayDay = timeinfo.tm_mday;
    int todayMonth = timeinfo.tm_mon + 1;

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() < 5)
            continue; // Skip lege regels

        // Format: Naam,DD-MM-JJJJ,G (waar G optioneel is en M/V kan zijn)
        int firstComma = line.indexOf(',');
        int secondDash = line.lastIndexOf('-');

        if (firstComma != -1)
        {
            String name = line.substring(0, firstComma);
            String datePart = line.substring(firstComma + 1);

            int d, m, j;
            char g = '?'; // Standaard waarde voor als er geen gender is ingevuld
            if (sscanf(datePart.c_str(), "%d-%d-%d,%c", &j, &m, &d, &g) >= 3)
            {
                // De check of het binnen 14 dagen valt is iets complexer omdat we ook rekening moeten houden met maandwisselingen
                if (m == todayMonth && d >= todayDay && d <= todayDay + 14)
                {
                    currentBirthdays.push_back({name, d, m, j, d - todayDay, g});
                }
            }
        }
    }
    file.close();
}

std::vector<BirthdayEntry> App::getSortedBirthdays(int limit = 5)
{
    std::vector<BirthdayEntry> alleVerjaardagen;
    File file = LittleFS.open("/birthdays.txt", "r");
    if (!file)
        return alleVerjaardagen;

    time_t nu;
    time(&nu);
    struct tm *tm_nu = localtime(&nu);

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        int comma = line.indexOf(',');
        if (comma == -1)
            continue;

        BirthdayEntry b;
        b.naam = line.substring(0, comma);
        String datePart = line.substring(comma + 1);
        b.gender = '?'; // Standaard fallback
        // if (sscanf(datePart.c_str(), "%d-%d-%d", &b.jaar, &b.maand, &b.dag) != 3)
        //     continue;
        // We proberen nu 4 waarden te lezen: J, M, D en de gender letter
        sscanf(datePart.c_str(), "%d-%d-%d,%c", &b.jaar, &b.maand, &b.dag, &b.gender);

        // Serial.printf("[BIRTHDAYS] Gelezen: %s, %d-%02d-%02d, Gender: %c\n", b.naam.c_str(), b.jaar, b.maand, b.dag, b.gender);
        
        // De check of de datum klopt (minimaal 3 waarden gelezen)
        if (b.jaar < 1900) continue; // Simpele validatie

        // Bereken eerstvolgende verjaardag
        struct tm tm_bday = *tm_nu;
        tm_bday.tm_mday = b.dag;
        tm_bday.tm_mon = b.maand - 1;
        tm_bday.tm_hour = 0;
        tm_bday.tm_min = 0;
        tm_bday.tm_sec = 0;

        time_t bday_t = mktime(&tm_bday);

        // Als de datum al geweest is dit jaar, verplaats naar volgend jaar
        if (difftime(bday_t, nu) < -86400)
        { // -1 dag marge voor verjaardag vandaag
            tm_bday.tm_year += 1;
            bday_t = mktime(&tm_bday);
        }

        b.dagenTeGaan = (long)(difftime(bday_t, nu) / 86400);
        b.nieuweLeeftijd = (tm_bday.tm_year + 1900) - b.jaar;

        alleVerjaardagen.push_back(b);
    }
    file.close();

    // Sorteer op dagenTeGaan
    std::sort(alleVerjaardagen.begin(), alleVerjaardagen.end(), [](const BirthdayEntry &a, const BirthdayEntry &b)
              { return a.dagenTeGaan < b.dagenTeGaan; });

    if (alleVerjaardagen.size() > limit)
        alleVerjaardagen.resize(limit);
    return alleVerjaardagen;
}

void App::updateGlobalBirthdayState() {
    // We halen de gesorteerde lijst op (zoals we eerder maakten)
    auto lijst = App::getSortedBirthdays(1); 

    if (!lijst.empty() && lijst[0].dagenTeGaan <= 5) {
        state.display.birthday_upcoming = true;
        state.display.birthday_days_until = lijst[0].dagenTeGaan;
        state.display.birthday_gender = lijst[0].gender;
        state.display.birthday_name = lijst[0].naam;
        Serial.printf("[BDAY] Match gevonden: %s (%c)\n", lijst[0].naam.c_str(), lijst[0].gender);
    } else {
        state.display.birthday_upcoming = false;
        state.display.birthday_days_until = -1;
        state.display.birthday_gender = '?';
        state.display.birthday_name = "";
    }
}
