/*
 * (c)2026 R van Dorland - Licensed under MIT License
 */

#include "display_logic.h"
#include "clasic_clock.h"
#include "config.h"

#include "global_data.h"
#include "helpers.h"
#include "leeftijd_calc.h" // Voor het eitje
#include "secret.h"        // Voor de namen/bday
#include "weather_logic.h" // Voor de forecast data

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <SPI.h>

// Forward declaration
void drawWebConfigQRinDataPaneel();

// 1. Hardware instanties (MOETEN hier gedefinieerd worden)
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite clkSpr = TFT_eSprite(&tft);
TFT_eSprite datSpr = TFT_eSprite(&tft);
TFT_eSprite datSpr2 = TFT_eSprite(&tft);
TFT_eSprite tckSpr = TFT_eSprite(&tft);

std::vector<TickerSegment> tickerSegments;

void addSegment(const char *text, uint16_t color);
void makeUpperCase(char *str);
void manageDataPanels();
void manageEasterEggTimer();
void manageServerTimeout();
void performTransition();
void renderTicker();
void updateClock();
void updateTickerSegments();
void vulAlertSegmenten();
void vulBootSegmenten();
void vulConfigSegmenten();
void vulEasterEggTekst(char *buffer, size_t bufferSize, int gDag, int gMaand, int gJaar, int forceVariant = -1);
bool vulGepersonaliseerdFeitje(char *buffer, size_t bufferSize);
void vulNormalSegmenten();

int berekenMinutenTotUpdate();

void makeUpperCase(char *str)
{
    for (int i = 0; str[i]; i++)
    {
        str[i] = toupper((unsigned char)str[i]);
    }
}

//          --- LOGICA VOOR HET VULLEN VAN DE TICKER-SEGMENTEN (BOOT, ALERT, NORMAL) ---
void vulBootSegmenten()
{
    TickerSegment s;
    s.color = TFT_YELLOW;

    // Geboortedatum ontleden uit de state
    int gJ, gM, gD;
    if (sscanf(state.user.dob.c_str(), "%d-%d-%d", &gJ, &gM, &gD) == 3)
    {
        char leeftijdBuf[90];
        // We roepen je calc-functie aan met forceVariant 0
        vulEasterEggTekst(leeftijdBuf, sizeof(leeftijdBuf), gD, gM, gJ, 0);

        snprintf(s.text, sizeof(s.text), "WELKOM %s! EVEN GEDULD VOOR DE WEER-UPDATE... EEN FIJTJE: %s",
                 state.user.name.c_str(), leeftijdBuf);
    }
    else
    {
        // Fallback als er geen geboortedatum is
        snprintf(s.text, sizeof(s.text), "WELKOM %s! EVEN GEDULD VOOR DE WEER-UPDATE...", state.user.name.c_str());
    }

    makeUpperCase(s.text);
    s.width = tckSpr.textWidth(s.text);
    tickerSegments.push_back(s);
}

void vulConfigSegmenten()
{
    TickerSegment s;
    s.color = TFT_ORANGE;

    // Kort en krachtig: geen afleiding
    snprintf(s.text, sizeof(s.text), " *** CONFIG ACTIEF: %s | QR-CODE: TOUCH 2 SEC *** ",
             WiFi.localIP().toString().c_str());

    makeUpperCase(s.text);
    s.width = tckSpr.textWidth(s.text);
    tickerSegments.push_back(s);
}

void vulAlertSegmenten()
{
    // PRIORITEIT: Als force_alert actief is EN er is tekst, toon dan ALLEEN het alarm
    if (state.weather.alert_text.length() > 0)
    {
        TickerSegment sAlert;
        sAlert.color = state.env.alert_active_color;
        snprintf(sAlert.text, sizeof(sAlert.text), " !!! ALARM: %s !!! ", state.weather.alert_text.c_str());
        // Maak uppercase
        makeUpperCase(sAlert.text);

        sAlert.width = tckSpr.textWidth(sAlert.text);
        tickerSegments.push_back(sAlert);
    }
    else
    {
        // Er is een verzoek om het alarm te tonen, maar er is geen tekst. Toon dan een generieke melding.
        TickerSegment sTemps;
        sTemps.color = state.env.ticker_color;
        snprintf(sTemps.text, sizeof(sTemps.text), "WEATHER TODAY: TEMP. MIN: %.1fC  |  MAX: %.1fC, MORNING: %.1fC, EVENING: %.1fC, NIGHT: %.1fC",
                 state.weather.today.temp_min,
                 state.weather.today.temp_max,
                 state.weather.today.temp_morn,
                 state.weather.today.temp_eve,
                 state.weather.today.temp_night);
        makeUpperCase(sTemps.text);
        sTemps.width = tckSpr.textWidth(sTemps.text);
        tickerSegments.push_back(sTemps);

        TickerSegment sSummary;
        sSummary.color = state.env.ticker_color;
        snprintf(sSummary.text, sizeof(sSummary.text), "| SUMMARY: %s",
                 state.weather.today.summary);
        makeUpperCase(sSummary.text);
        sSummary.width = tckSpr.textWidth(sSummary.text);
        tickerSegments.push_back(sSummary);
    }
}

void vulNormalSegmenten()
{
    TickerSegment sWeer;
    sWeer.color = state.env.ticker_color;
    snprintf(sWeer.text, sizeof(sWeer.text), "WEER VANDAAG: %s, TEMP: MIN: %.1fC  |  MAX: %.1fC",
             state.weather.description.c_str(),
             state.weather.today.temp_min,
             state.weather.today.temp_max);
    makeUpperCase(sWeer.text);
    sWeer.width = tckSpr.textWidth(sWeer.text);
    tickerSegments.push_back(sWeer);

    TickerSegment sGevoelstemp;
    sGevoelstemp.color = state.env.ticker_color;
    snprintf(sGevoelstemp.text, sizeof(sGevoelstemp.text), "| GEVOELSTEMP: %.1fC", state.weather.feels_like);
    makeUpperCase(sGevoelstemp.text);
    sGevoelstemp.width = tckSpr.textWidth(sGevoelstemp.text);
    tickerSegments.push_back(sGevoelstemp);

    TickerSegment sVocht;
    sVocht.color = state.env.ticker_color;
    snprintf(sVocht.text, sizeof(sVocht.text), "| LUCHTVOCHTIGHEID: %d%%", (int)state.weather.humidity);
    makeUpperCase(sVocht.text);
    sVocht.width = tckSpr.textWidth(sVocht.text);
    tickerSegments.push_back(sVocht);

    TickerSegment sDauwpunt;
    sDauwpunt.color = state.env.ticker_color;
    snprintf(sDauwpunt.text, sizeof(sDauwpunt.text), "| DAUWPUNT: %.1fC",
             //  state.weather.location.c_str(),
             state.weather.dew_point);
    sDauwpunt.width = tckSpr.textWidth(sDauwpunt.text);
    tickerSegments.push_back(sDauwpunt);

    TickerSegment sUVI;
    sUVI.color = state.env.ticker_color;
    snprintf(sUVI.text, sizeof(sUVI.text), "| UV-INDEX: %.1f", state.weather.uvi);
    makeUpperCase(sUVI.text);
    sUVI.width = tckSpr.textWidth(sUVI.text);
    tickerSegments.push_back(sUVI);

    TickerSegment sBewolking;
    sBewolking.color = state.env.ticker_color;
    snprintf(sBewolking.text, sizeof(sBewolking.text), "| BEWOLKING: %.d%%",
             //  state.weather.location.c_str(),
             (int)state.weather.clouds);
    sBewolking.width = tckSpr.textWidth(sBewolking.text);
    tickerSegments.push_back(sBewolking);

    TickerSegment sZicht;
    sZicht.color = state.env.ticker_color;
    float zichtWaarde = (state.weather.visibility >= 1000) ? (state.weather.visibility / 1000.0f) : (float)state.weather.visibility;
    const char *eenheid = (state.weather.visibility >= 1000) ? "km" : "m";
    snprintf(sZicht.text, sizeof(sZicht.text), "| ZICHT: %.1f %s", zichtWaarde, eenheid);
    sZicht.width = tckSpr.textWidth(sZicht.text);
    tickerSegments.push_back(sZicht);

    TickerSegment sWind;
    sWind.color = state.env.ticker_color;
    float speedMs = state.weather.wind_speed / 0.27778f;
    snprintf(sWind.text, sizeof(sWind.text), "| WIND: %.1f m/s (%s)",
             speedMs,
             getWindRoos(state.weather.wind_deg).c_str());
    sWind.width = tckSpr.textWidth(sWind.text);
    tickerSegments.push_back(sWind);

    float gustMs = state.weather.wind_gust / 0.27778f;
    if (gustMs > speedMs + 1.5f)
    {
        TickerSegment sGust;
        sGust.color = state.env.ticker_color;
        snprintf(sGust.text, sizeof(sGust.text), "| WINDVLAGEN: %.1f m/s", gustMs);
        sGust.width = tckSpr.textWidth(sGust.text);
        tickerSegments.push_back(sGust);
    }

    // --- SEGMENT 3: PAASEI (De Verrassing) ---
    if (state.display.show_easter_egg)
    {
        TickerSegment sEasterEgg;
        if (vulGepersonaliseerdFeitje(sEasterEgg.text, sizeof(sEasterEgg.text)))
        {
            sEasterEgg.color = TFT_YELLOW;
            sEasterEgg.width = tckSpr.textWidth(sEasterEgg.text);
            tickerSegments.push_back(sEasterEgg);
            // DE GRENDEL: Verbruik de vlag direct!
            state.display.show_easter_egg = false;
            Serial.println(F("[TICKER] Paasei toegevoegd aan de Segmenten."));
        }
    }

    // // Segment: Wachttijd tot volgende update
    // int minResterend = berekenMinutenTotUpdate();
    // if (minResterend > 0)
    // {
    //     TickerSegment sWait;
    //     sWait.color = TFT_CYAN;
    //     snprintf(sWait.text, sizeof(sWait.text), " | UPDATE OVER %d MIN", minResterend);
    //     sWait.width = tckSpr.textWidth(sWait.text);
    //     tickerSegments.push_back(sWait);
    // }

    // Afsluiter
    TickerSegment sEnd;
    sEnd.color = TFT_GOLD;
    strncpy(sEnd.text, "  +++  ", sizeof(sEnd.text));
    sEnd.width = tckSpr.textWidth(sEnd.text);
    tickerSegments.push_back(sEnd);
}

// Helper om segmenten toe te voegen aan de state
void addSegment(String t, uint16_t c)
{
    TickerSegment seg; // 1. Maak een leeg object

    // 2. Kopieer de tekst veilig naar de char array
    // We gebruiken strncpy om te voorkomen dat we buiten de 128 bytes schrijven
    strncpy(seg.text, t.c_str(), sizeof(seg.text) - 1);
    seg.text[sizeof(seg.text) - 1] = '\0'; // Altijd afsluiten met een null-terminator

    seg.color = c;
    seg.width = tft.textWidth(seg.text); // Gebruik de nieuwe char array voor de breedte

    // 3. Nu de vector vullen met het complete object
    state.ticker_segments.push_back(seg);
}

void updateTickerSegments()
{
    static TickerMode oudeMode = MODE_BOOT;

    // 1. TIMER CHECK (Alarm automatisch uit na 10 min)
    if (state.display.force_alert_display && millis() > state.display.alert_show_until)
    {
        state.display.force_alert_display = false;
        state.display.alert_show_until = 0;
        state.display.force_ticker_refresh = true;
    }

    // 2. DE BESLISSER (De Pikorde / Prioriteit)
    TickerMode nieuweMode;
    if (state.network.web_server_active)
    {
        nieuweMode = MODE_CONFIG; // Gebruik de exclusieve Config modus
    }
    else if (!state.weather.data_is_fresh)
    {
        nieuweMode = MODE_BOOT;
    }
    else if (state.display.force_alert_display)
    {
        nieuweMode = MODE_ALERT;
    }
    else
    {
        nieuweMode = MODE_NORMAL;
    }

    // 3. WISSEL LOGICA (Start van rechts bij nieuwe modus)
    if (nieuweMode != oudeMode || state.display.force_ticker_refresh)
    {
        state.display.ticker_x = tft.width(); // Reset naar 320
        oudeMode = nieuweMode;
        state.display.force_ticker_refresh = false;
        Serial.printf("[TICKER] Wissel naar Mode: %d\n", nieuweMode);
    }

    // 4. DE SCHOONMAAK (Cruciaal: Eerst alles leegmaken!)
    tickerSegments.clear();

    // 5. DE INVULLING (Switch-case op basis van de gekozen mode)
    switch (nieuweMode)
    {
    case MODE_BOOT:
        vulBootSegmenten();
        break;
    case MODE_CONFIG:
        vulConfigSegmenten();
        break;
    case MODE_ALERT:
        vulAlertSegmenten();
        break;
    case MODE_NORMAL:
        vulNormalSegmenten();
        break;
    }

    // 6. BREEDTE BEREKENING (Stopt de race)
    uint16_t nieuweBreedte = 0;
    for (auto &s : tickerSegments)
    {
        nieuweBreedte += s.width;
    }
    state.display.total_ticker_width = nieuweBreedte;

    // Serial.printf("[TICKER] Totaal segmenten: %d, Totale breedte: %d\n", (int)tickerSegments.size(), nieuweBreedte);
}

//          --- LOGICA VOOR HET BEHEREN VAN HET PAASEI-TIMER EN DE GEKOPPELEDE TEKST ---
void manageEasterEggTimer()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    int huidigeMinuutInDag = (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    // 1. KIEZEN VAN EEN NIEUW MOMENT
    if (state.env.next_easter_egg_minute == -1)
    {
        int interval = random(5, 56);
        state.env.next_easter_egg_minute = (huidigeMinuutInDag + interval) % 1440;
        Serial.printf(F("[PAASEI] Volgende over % d min (omstreeks minuut % d)\n"),
                      interval, state.env.next_easter_egg_minute);
    }

    // 2. IS HET TIJD? (De Grendel)
    if (huidigeMinuutInDag == state.env.next_easter_egg_minute)
    {
        state.display.show_easter_egg = true;

        // CRUCIAAL: Zet hem direct op -1 zodat hij deze minuut NIET nog 1000x checkt!
        state.env.next_easter_egg_minute = -1;

        Serial.println(F("[PAASEI] Vlag omhoog en timer gereset voor de volgende ronde."));
    }
}

//          --- LOGICA VOOR HET RENDEREN EN ANIMEREN VAN DE TICKER ---
void renderTicker()
{
    // 1. VOORBEREIDING
    tckSpr.fillSprite(TFT_BLACK);
    tckSpr.setTextFont(2);
    tckSpr.setTextDatum(TL_DATUM);

    // 2. BEREKEN TOTALE BREEDTE (S.P.o.T.)
    // We moeten weten hoe breed alle segmenten samen zijn voor de wissel-logica
    int berekendeBreedte = 0;
    for (auto &seg : tickerSegments)
    {
        berekendeBreedte += seg.width + 14; // 20px spatie
    }
    state.display.total_ticker_width = berekendeBreedte;

    // 3. TEKEN DE SEGMENTEN (Set A en Set B voor naadloze loop)
    int startX = state.display.ticker_x;

    // Teken twee keer de hele rits segmenten achter elkaar
    for (int i = 0; i < 2; i++)
    {
        int segmentX = startX + (i * state.display.total_ticker_width);
        for (auto &seg : tickerSegments)
        {
            tckSpr.setTextColor(seg.color, TFT_BLACK);
            tckSpr.drawString((char *)seg.text, segmentX, 0);
            segmentX += seg.width + 14;
        }
    }

    // 4. PUSH NAAR SCHERM (SLECHTS ÉÉN KEER!)
    tckSpr.pushSprite(0, Config::get_ticker_y(tft.height()));

    // 5. ANIMATIE STAP

    // 1. Beweeg de ticker
    state.display.ticker_x -= 1;

    // 1. DE NORMALE DOORLOOP (Naadloos)
    if (state.display.ticker_x <= -state.display.total_ticker_width)
    {
        // We houden de positie op 0 zodat hij direct weer links begint
        state.display.ticker_x = 0;

        // We verversen de segmenten (als de data in de tussentijd is bijgewerkt)
        updateTickerSegments();
    }

    // 2. DE GEFORCEERDE WISSEL (Bijv. bij Alarm of na WebConfig)
    if (state.display.force_ticker_refresh)
    {
        // Zet de ticker weer helemaal rechts buiten beeld voor een frisse start
        state.display.ticker_x = tft.width(); 
        
        // Ververs de inhoud direct met de mogelijk nieuwe data uit de WebConfig
        updateTickerSegments();
        
        state.display.force_ticker_refresh = false;
        
        // Optioneel: Forceer ook een update van de andere panelen
        // manageDataPanels(); 
    }
}

//          --- PANEEL LOGIC: Beheer van de data-panelen en de overgangen daartussen ---
void performTransition(TFT_eSprite *oldSpr, TFT_eSprite *newSpr)
{

    if (oldSpr == nullptr || newSpr == nullptr)
        return;
    if (oldSpr->width() == 0 || newSpr->width() == 0)
    {
        newSpr->pushSprite(Config::data_x, Config::data_y);
        return;
    }

    int w = oldSpr->width();
    int h = oldSpr->height();
    int x = Config::data_x;
    int y = Config::data_y;
    int speed = state.display.transition_speed;
    // VEILIGHEID: Als speed 0 of lager is, forceer een geldige waarde
    if (speed <= 0)
        speed = 8;

    // --- DE MAGIE: Zet het 'hekje' om het data-vlak ---
    // Alles wat buiten deze coördinaten getekend wordt, wordt genegeerd.
    tft.setViewport(x, y, w, h);

    for (int i = 0; i <= ((state.display.transition_type >= 2) ? h : w); i += speed)
    {
        // Binnen de viewport is (x, y) nu (0, 0)
        switch (state.display.transition_type)
        {
        case 0: // SLIDE_LEFT
            oldSpr->pushSprite(-i, 0);
            newSpr->pushSprite(w - i, 0);
            break;
        case 1: // SLIDE_RIGHT
            oldSpr->pushSprite(i, 0);
            newSpr->pushSprite(-w + i, 0);
            break;
        case 2: // SLIDE_UP
            oldSpr->pushSprite(0, -i);
            newSpr->pushSprite(0, h - i);
            break;
        case 3: // SLIDE_DOWN
            oldSpr->pushSprite(0, i);
            newSpr->pushSprite(0, -h + i);
            break;
        default:
            newSpr->pushSprite(0, 0);
            i = 9999; // Stop loop
            break;
        }
        tft.resetViewport();

        // We verplaatsen de ticker-positie handmatig met 1 pixel
        // (of hetzelfde tempo als de 'speed' van de wissel)
        state.display.ticker_x -= 1;

        renderTicker();
        updateClock();
        tft.setViewport(x, y, w, h); // Terug naar het 'hekje' voor de volgende frame

        yield();
    }

    // --- RESET: Haal het hekje weer weg voor de rest van het scherm ---
    tft.resetViewport();
    newSpr->pushSprite(x, y);
}

//          --- HOOFDFUNCTIE: Beheer van de data-panelen en de wissel daartussen ---
void manageDataPanels()
{
    // static bool lastQRState = false;

    // if (state.network.web_server_active && state.display.show_config_qr)
    // {
    //     // Alleen de ALLEREERSTE keer tekenen als de vlag op true gaat
    //     if (!lastQRState)
    //     {
    //         drawWebConfigQRinDataPaneel();
    //         lastQRState = true;
    //         Serial.println(F("[PANEEL] QR-code eenmalig getekend. Rust hersteld."));
    //     }
    //     return; // We doen NIKS meer zolang de vlag true is
    // }
    // lastQRState = false; // Reset de vlag zodra de QR uit gaat

    // // Als de server niet meer actief is, zorgen we dat de vlag ook weer op false gaat
    // if (!state.network.web_server_active)
    // {
    //     state.display.show_config_qr = false;
    // }

    // SPoT-Check: Als we in een andere modus zitten, doen we NIETS
    if (state.network.is_setup_mode || state.network.web_server_active || state.display.show_system_monitor)
    {
        return; 
    }


    // 1. Check of we überhaupt al een succesvolle update hebben gehad
    if (!state.weather.data_is_fresh)
    {
        // We zitten in de opstartfase: Toon geforceerd het 'Vandaag' paneel
        state.display.show_vandaag = true;
        state.display.last_panel_switch = millis(); // Reset de timer

        // Teken het paneel statisch zonder transitie-gedoe
        updateDataPaneelVandaag();
        datSpr.pushSprite(Config::data_x, Config::data_y);
        return;
    }

    // 2. Check of OWM geconfigureerd is (als extra veiligheid)
    if (state.network.owm_key.length() < 10)
    {
        showSetupInstructionPanel();
        return;
    }

    // 3. NORMALE DEURSTUURING: Wissel tussen Vandaag en Komende Dagen
    unsigned long nu = millis();
    unsigned long interval = state.display.show_vandaag ? Config::time_display_long : Config::time_display_short;

    // Serial.printf("Panel: %d, Delta: %lu, Target: %lu\n", state.display.show_vandaag, millis() - state.display.last_panel_switch, interval);

    // 1. Is het tijd voor de wissel?
    if (nu - state.display.last_panel_switch > interval)
    {
        // Stoplicht check: Geen animatie tijdens WiFi-updates
        if (state.network.is_updating)
            return;

        if (state.display.show_vandaag)
        {
            // Serial.println(F(">>> Start wissel naar Forecast"));
            updateDataPaneelForecast();

            // STAP B: Korte pauze (10-20ms) zodat de ESP32 de sprite-buffer kan afsluiten
            delay(10);

            performTransition(&datSpr, &datSpr2);
            state.display.show_vandaag = false;
        }
        else
        {
            // Serial.println(F(">>> Start wissel naar Vandaag"));
            updateDataPaneelVandaag();

            // STAP B: Korte pauze (10-20ms) zodat de ESP32 de sprite-buffer kan afsluiten
            delay(10);

            performTransition(&datSpr2, &datSpr);
            state.display.show_vandaag = true;
        }

        state.display.last_panel_switch = nu;
        return; // Verlaat functie, we zijn klaar voor deze frame
    }

    // 2. Statische Update (Alleen als er geen netwerk-activiteit is)
    if (!state.network.is_updating)
    {
        if (state.display.show_vandaag)
        {
            updateDataPaneelVandaag();
            datSpr.pushSprite(Config::data_x, Config::data_y);
        }
        else
        {
            updateDataPaneelForecast();
            datSpr2.pushSprite(Config::data_x, Config::data_y);
        }
    }
}
