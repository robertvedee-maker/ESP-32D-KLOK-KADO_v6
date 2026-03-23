/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Main application logic
 * Hierin staat de setup() en loop() functie, en worden de belangrijkste functies aangeroepen die in de andere files staan.
 * We hebben ervoor gekozen om deze functies in een aparte file te zetten om de code beter te organiseren en de main loop overzichtelijk te houden.
 */

#include "app_actions.h"


class TFT_eSprite; 
extern TFT_eSPI tft;

static uint32_t lastLoggedStartTime = 0;

unsigned long lastWeatherUpdate = 0;
unsigned long lastSensorUpdate = 0;
RTC_DATA_ATTR uint32_t lastOWMFetchTime = 0; // RTC_DATA_ATTR uint32_t lastSensorFetchTime = 0; // Voor het bijhouden van de laatste sensor-update tijd (overleeft reboots)

namespace Config
{
    bool forceFirstWeatherUpdate = true;
}

void setup()
{
    // 1. Hou alles even STROOMLOOS (Buck uit)
    pinMode(Config::pin_buck_enable, OUTPUT);
    digitalWrite(Config::pin_buck_enable, HIGH);
    delay(100); // Korte rust voor de condensatoren

    pinMode(Config::pin_fingerprint_led, OUTPUT);   // De Systeem-LED achter de vingerprint
    digitalWrite(Config::pin_fingerprint_led, LOW); // Start uit

    Serial.begin(115200);
    delay(200); // 0.2 seconde pauze voor stabilisatie

    Serial.printf("\n--- Systeem Start ---\n");

    Preferences prefs;
    prefs.begin("config", false);
    // --- HANDMATIGE DATA INJECTIE ---
    Serial.printf("Bezig met handmatige data injectie...\n");
    prefs.putString("owm_key", SECRET_OMW3);
    prefs.putDouble("lat", SECRET_LAT);
    prefs.putDouble("lon", SECRET_LON);
    prefs.end();
    // --- EINDE INJECTIE ---

    // DISPLAY START
    App::setupDisplay();

    // 2. Zet de stroom erop (Buck aan)
    digitalWrite(Config::pin_buck_enable, LOW);

    // 3. CRUCIAAL: Wacht tot de sensoren en het scherm 'wakker' zijn
    // De BMP280 heeft ongeveer 10-50ms nodig, de AHT20 ook.
    delay(200); // We geven het systeem een kwart seconde om volledig op te starten voordat we verder gaan, voor maximale stabiliteit

    // // TIJDELIJKE TEST-STOP
    // drawSetupModeActive();
    // updateDisplayBrightness(state.display.day_brightness); // Licht aan!

    // while (true)
    // {
    //     yield(); // Blijf hier voor eeuwig hangen om te kijken/meten
    // }

    // STORAGE START (Vroeger, zodat we direct de opgeslagen helderheid kunnen laden)
    App::initStorage();

    // SENSOR START
    if (App::setupSensors())
    {
        App::handleSensors(); // Doe direct de eerste meting
    }

    // WiFi Setup
    App::setupWiFi();
    App::drawNetworkInfo(); // Laat het IP-adres kort op het scherm zien

    // Tijd configureren (NTP)
    configTzTime(SECRET_TZ_INFO, SECRET_NTP_SERVER);
    App::manageTimeFunctions(); // Bepaalt zon-tijden en vult state.env

    if (prefs.begin("config", true))
    { // open in read-only
        state.network.owm_key = prefs.getString("owm_key", "");
        state.env.lat = prefs.getDouble("lat", 0.0);
        state.env.lon = prefs.getDouble("lon", 0.0);
        prefs.end();
        // Serial.printf("[DEBUG] State handmatig gevuld vanuit Preferences\n");
    }
    else
    {
        Serial.printf("[DEBUG] FOUT: Kon Preferences 'config' niet openen!\n");
    }

    if (!LittleFS.begin(true))
    { // 'true' formatteert automatisch als mounten mislukt
        Serial.println("LittleFS Mount Failed");
    }
    else
    {
        Serial.println("LittleFS Gemount!");
        // Check vrije ruimte (puur voor debug)
        Serial.printf("Totaal: %u KB, Gebruikt: %u KB\n",
                      LittleFS.totalBytes() / 1024,
                      LittleFS.usedBytes() / 1024);
    }

    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    Serial.printf("--- Systeem Start Voltooid --- | tijd: %02d:%02d\n", timeinfo->tm_hour, timeinfo->tm_min);

    // HELDERHEID TOEPASSEN (Hardware-fade naar de geladen waarde)
    // updateDisplayBrightness(state.display.day_brightness);
    App::finalizeSetupAndShowDashboard();
    App::updateBirthdayAlertState();

    // // Preferences prefs;
    // prefs.begin("config", true); // 'true' betekent read-only

    // Serial.printf("--- Inhoud van Preferences ---\n");
    // Serial.printf("SSID: %s\n", prefs.getString("ssid", "NIET GEVONDEN").c_str());
    // Serial.printf("OWM Key: %s\n", prefs.getString("owm_key", "NIET GEVONDEN").c_str());
    // Serial.printf("Lat: %.6f\n", prefs.getDouble("lat", 0.0));

    // Serial.printf("Lon: %.6f\n", prefs.getDouble("lon", 0.0));
    // prefs.end();
}

static unsigned long lastSetupRedraw = 0;

void loop()
{
    // manageTimeFunctions();
    App::evaluateSystemSafety();

    // Herstart check
    if (state.network.pending_restart)
    {
        Serial.printf("[SYSTEM] Licht uit, herstarten...\n");
        tft.fillScreen(TFT_BLACK);                   // 1. Maak het scherm zwart (softwarematig)
        digitalWrite(Config::pin_buck_enable, HIGH); // 2. Trek de stekker uit de Buck (hardwarematig)
        delay(100);                                  // 3. Korte pauze om de elco's van de Buck leeg te laten lopen
        ESP.restart();                               // 4. De echte herstart

    }

    // 1. TOUCH & LED (Altijd eerst voor maximale responsiviteit)
    static unsigned long lastTouchTick = 0;
    if (millis() - lastTouchTick > 50)
    {
        App::handleTouchLadder();
        App::manageStatusLed();
        lastTouchTick = millis();
    }

    // Check of we nog in de verjaardagskalender modus zitten, en zo ja, of deze al weer uitgezet moet worden
    if (state.display.show_calendar && millis() > state.display.calendar_show_until) {
    state.display.show_calendar = false;
    tft.fillScreen(TFT_BLACK); // Forceer refresh naar weer-layout
    }

    // 2. SETUP / BEHEER MODUS (De "Blokkerende" Mode)
    // We checken of de setup-vlag óf de webserver actief is
    if (state.network.is_setup_mode || state.network.web_server_active)
    {
        // Ververs het scherm 1x per seconde voor de aftelbalk en status
        if (millis() - lastSetupRedraw > 1000)
        {
            App::drawSetupModeActive();
            lastSetupRedraw = millis();
        }

        // Check of de 10 minuten om zijn
        App::manageServerTimeout();

        yield();
        if (state.network.server_start_time != lastLoggedStartTime)
        {
            Serial.printf("[SYSTEM] server start time set to: %lu\n", state.network.server_start_time);
            lastLoggedStartTime = state.network.server_start_time;
        }
        return; // Stop de rest van de loop om Heap te sparen voor de Webserver
    }

    // --- 3. SYSTEM MONITOR MODUS (Blokkerend) ---
    if (state.display.show_system_monitor)
    {
        static unsigned long lastMonitorUpdate = 0;
        if (millis() - lastMonitorUpdate > 500)
        {
            App::drawSystemMonitor();
            lastMonitorUpdate = millis();
        }
        yield();
        return; // Sla het dashboard over als de monitor aan staat
    }

    // 3. NORMALE KLOK-OPERATIE (Alleen als setup NIET actief is)

    // Achtergrond taken (elke 10 sec)
    static unsigned long lastTenSecondTick = 0;
    if (millis() - lastTenSecondTick > 10000)
    {
        App::handleSensors();
        App::handleWiFiEco();
        App::manageBrightness();
        App::manageTimeFunctions();
        App::manageEasterEggTimer();
        App::updateBaroTrend();
        App::checkDailyTriggers();
        lastTenSecondTick = millis();
    }

    static unsigned long lastBirthdayCheck = 0;
    if (millis() - lastBirthdayCheck > 60 * 60 * 1000) 
    {// Check elke uur op verjaardagen, dit is ruim genoeg gezien we ook een 'dichtbij' vlag hebben die elke 10 sec wordt geüpdatet
        App::updateBirthdayAlertState();
        lastBirthdayCheck = millis();
    }


    // --- BLOK 6: VISUEEL (KLOK, TICKER & ANIMATIE) ---
    static time_t last_now = 0;
    time_t now = time(nullptr);
    if (now != last_now)
    {// Klok update (elke sec)
        last_now = now;
        App::updateClock();
    }

    App::renderTicker();
    App::manageDataPanels();
    App::manageWeatherUpdates();
}
