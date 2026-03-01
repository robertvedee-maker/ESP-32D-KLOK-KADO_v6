/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Main application logic
 * Hierin staat de setup() en loop() functie, en worden de belangrijkste functies aangeroepen die in de andere files staan.
 * We hebben ervoor gekozen om deze functies in een aparte file te zetten om de code beter te organiseren en de main loop overzichtelijk te houden.
 */

#include "config.h"        // Voor pinnen en constanten
#include "daynight.h"      // Voor tijd en helderheid
#include "display_logic.h" // Voor ticker en panelen
#include "env_sensors.h"   // Voor BME/AHT sensoren
#include "global_data.h"   // Voor toegang tot 'state'
#include "helpers.h"       // Voor setupDisplay, updateClock, etc.
#include "network_logic.h" // Voor WiFi en OTA setup
#include "secret.h"        // Voor WiFi credentials
#include "storage_logic.h" // Voor NVS opslag
#include "weather_logic.h" // Voor OWM data
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <time.h>
#include <WiFi.h>

void updateTickerSegments();
void evaluateSystemSafety();
void finalizeSetupAndShowDashboard();
void handleTouchLadder();
void loop();
void manageDataPanels();
void manageEasterEggTimer();
void manageStatusLed();
void renderTicker();
void setup();
void showSetupInstructionPanel();

unsigned long lastWeatherUpdate = 0;
unsigned long lastSensorUpdate = 0;
RTC_DATA_ATTR uint32_t lastOWMFetchTime = 0; // RTC_DATA_ATTR uint32_t lastSensorFetchTime = 0; // Voor het bijhouden van de laatste sensor-update tijd (overleeft reboots)

namespace Config
{
    bool forceFirstWeatherUpdate = true;
}

void setup()
{

    // TIJDELIJK: Wis alles om de 'eerste keer' te simuleren
    // #include <nvs_flash.h>
    // nvs_flash_erase();
    // nvs_flash_init();

    pinMode(Config::pin_fingerprint_led, OUTPUT);   // De Systeem-LED achter de vingerprint
    digitalWrite(Config::pin_fingerprint_led, LOW); // Start uit

    pinMode(Config::pin_buck_enable, OUTPUT);   // Buck converter control pin
    digitalWrite(Config::pin_buck_enable, LOW); // Start met buck converter aan (stroomvoorziening actief)

    Serial.begin(115200);
    delay(500); // 0.5 seconde pauze voor stabilisatie

    Serial.println(F("\n--- Systeem Start ---"));

    Preferences prefs;
    prefs.begin("config", false);
    // --- HANDMATIGE DATA INJECTIE ---
    Serial.println(F("Bezig met handmatige data injectie..."));
    prefs.putString("owm_key", SECRET_OMW3);
    prefs.putDouble("lat", SECRET_LAT);
    prefs.putDouble("lon", SECRET_LON);
    prefs.end();
    // --- EINDE INJECTIE ---

    // DISPLAY START
    setupDisplay();

    // // TIJDELIJKE TEST-STOP
    // drawSetupModeActive();
    // updateDisplayBrightness(state.display.day_brightness); // Licht aan!

    // while (true)
    // {
    //     yield(); // Blijf hier voor eeuwig hangen om te kijken/meten
    // }

    // STORAGE START (Vroeger, zodat we direct de opgeslagen helderheid kunnen laden)
    initStorage();

    // SENSOR START
    if (setupSensors())
    {
        handleSensors(); // Doe direct de eerste meting
    }

    // WiFi Setup
    setupWiFi();

    // Tijd configureren (NTP)
    configTzTime(SECRET_TZ_INFO, SECRET_NTP_SERVER);
    manageTimeFunctions(); // Bepaalt zon-tijden en vult state.env

    if (prefs.begin("config", true))
    { // open in read-only
        state.network.owm_key = prefs.getString("owm_key", "");
        state.env.lat = prefs.getDouble("lat", 0.0);
        state.env.lon = prefs.getDouble("lon", 0.0);
        prefs.end();
        // Serial.println(F("[DEBUG] State handmatig gevuld vanuit Preferences"));
    }
    else
    {
        Serial.println(F("[DEBUG] FOUT: Kon Preferences 'config' niet openen!"));
    }


    Serial.println(F("--- Systeem Start Voltooid ---"));

    // HELDERHEID TOEPASSEN (Hardware-fade naar de geladen waarde)
    // updateDisplayBrightness(state.display.day_brightness);
    finalizeSetupAndShowDashboard();

    // // Preferences prefs;
    // prefs.begin("config", true); // 'true' betekent read-only

    // Serial.println(F("--- Inhoud van Preferences ---"));
    // Serial.print(F("SSID: "));
    // Serial.println(prefs.getString("ssid", "NIET GEVONDEN"));
    // Serial.print(F("OWM Key: "));
    // Serial.println(prefs.getString("owm_key", "NIET GEVONDEN"));
    // Serial.print(F("Lat: "));
    // Serial.println(prefs.getDouble("lat", 0.0), 6);
    // Serial.print(F("Lon: "));
    // Serial.println(prefs.getDouble("lon", 0.0), 6);
    // prefs.end();
}

void loop()
{

    // static unsigned long debugTicker = 0;
    // if (millis() - debugTicker > 1000)
    // {
    //     Serial.printf(F("[STATUS] Ticker Segments: %d, X-Pos: %d\n"), tickerSegments.size(), state.display.ticker_x);
    //     debugTicker = millis();
    // }

    // In de normale loop
    static unsigned long lastTouchTick = 0;
    if (millis() - lastTouchTick > 30)
    {
        handleTouchLadder();
        manageStatusLed();
        lastTouchTick = millis();
    }

    // struct NetworkState {
    //     // ... bestaande velden ...
    //     bool ota_enabled = true; // Voor nu op true, later wordt dit false bij boot
    // };

    // // Alleen OTA afhandelen als de vlag aan staat
    // if (state.network.ota_enabled) {
    //     ArduinoOTA.handle();
    // }

    // --- BLOK 0 & 1 (REALTIME) ---

    if (state.network.pending_restart && millis() > state.network.restart_at)
    {
        Serial.println(F("[SYSTEM] Geplande herstart wordt nu uitgevoerd..."));
        ESP.restart();
    }

    if (state.network.is_setup_mode)
    {
        // Laat de achtergrond-taken (zoals de Webserver en DNS) hun werk doen
        yield();
        return;
    }

    // Update de globale status vlag zodat de rest van de code weet waar we aan toe zijn
    state.network.wifi_connected = (WiFi.status() == WL_CONNECTED);

    // ArduinoOTA.handle();

    // // --- 2. DE "GEEN DATA" LOCK ---
    // if (state.network.owm_key.length() < 10)
    // {
    //     // We tekenen ENKEL de klok en het instructie-paneel
    //     static time_t last_sec = 0;
    //     time_t nu = time(nullptr);
    //     if (nu != last_sec)
    //     {
    //         last_sec = nu;
    //         updateClock();               // Klok werkt altijd
    //         showSetupInstructionPanel(); // Toon IP op de plek van het weer
    //     }
    //     return; // STOP HIER: Doe geen ticker, geen panels, geen weer-fetch
    // }



    // --- BLOK 2: DE KLOK (ELKE SECONDE) ---
    static time_t last_now = 0;
    time_t now = time(nullptr);
    if (now != last_now)
    {
        last_now = now;
        updateClock();
    }

    // --- BLOK 3: SENSOREN & BEHEER (ELKE 10 SECONDEN) ---
    static unsigned long lastTenSecondTick = 0;
    if (millis() - lastTenSecondTick > 10000)
    {
        handleSensors();
        manageBrightness();
        manageTimeFunctions();
        manageServerTimeout();
        evaluateSystemSafety();
        manageEasterEggTimer();
        // Als we in de toekomst OTA willen, is dit ook een goed moment om even te checken
        // if (state.network.ota_enabled) { ArduinoOTA.handle(); }
        lastTenSecondTick = millis();

        // Serial.printf(F("[SYSTEM] Heap vrij: %u bytes\n"), ESP.getFreeHeap());
    }

    // --- BLOK 4: DATA UPDATES  ---
    manageWeatherUpdates();

    // --- BLOK 5: PANELEN & UI ---
    handleWiFiEco();

    // --- BLOK 6: VISUEEL (TICKER & ANIMATIE) ---
    // if (!state.display.is_animating && !state.network.is_updating)
    // {
    //     renderTicker();
    // }
    renderTicker();
    manageDataPanels();

    // handleServerControl();
    // --- S3 VOORBEREIDING (UITGECOMMENTARIEERD) ---
    /*
    if (state.network.ota_enabled) {
        ArduinoOTA.handle();
    }
    */

    // // Check voor geplande herstart
    // if (state.network.pending_restart && millis() > state.network.restart_at)
    // {
    //     Serial.println(F("[SYSTEM] Geplande herstart wordt nu uitgevoerd..."));
    //     ESP.restart();
    // }

    // --- DEBUG: Toon CPU temperatuur ---
    // tft.setTextFont(2);
    // tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    // tft.drawFloat(state.env.case_temp, 1, 5, 135); // Voor debug: Case temp

    // --- DEBUG: Toon vrije RAM in KB ---
    // tft.setTextFont(2);
    // tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    // tft.drawNumber(ESP.getFreeHeap() / 1024, 5, 135); // Toont vrije KB's
}