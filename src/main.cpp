/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Main application logic
 * Hierin staat de setup() en loop() functie, en worden de belangrijkste functies aangeroepen die in de andere files staan.
 * We hebben ervoor gekozen om deze functies in een aparte file te zetten om de code beter te organiseren en de main loop overzichtelijk te houden.
 * De functies in deze file worden aangeroepen vanuit app_actions.h, zodat we een centrale plek hebben voor alle 'acties' die de app kan uitvoeren.
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
void renderTicker();
void manageDataPanels();
void showSetupInstructionPanel();
void handleTouchLadder();
void manageStatusLed();
bool setupInternalTempSensor();
void updateInternalSensors();

unsigned long lastWeatherUpdate = 0;
unsigned long lastSensorUpdate = 0;

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

    pinMode(2, OUTPUT);   // De Systeem-LED achter de vingerprint
    digitalWrite(2, LOW); // Start uit

    Serial.begin(115200);
    delay(1500); // 1.5 seconde pauze voor stabilisatie

    Serial.println("\n--- Systeem Start ---");

    Preferences prefs;
    // Gebruik de naam van de namespace die jouw S.P.o.T. systeem gebruikt
    // Meestal is dit "config" of "settings"
    prefs.begin("config", false);

    // --- HANDMATIGE DATA INJECTIE ---
    // Serial.println("Bezig met handmatige data injectie...");

    prefs.putString("owm_key", SECRET_OMW3);
    // S.P.o.T. gebruikt vaak floats of doubles voor locatie
    prefs.putDouble("lat", SECRET_LAT);
    prefs.putDouble("lon", SECRET_LON);

    prefs.end();
    Serial.println("Injectie voltooid! Vergeet deze regels later niet te verwijderen.");
    // --- EINDE INJECTIE ---

    // DISPLAY START
    setupDisplay();
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

    if (prefs.begin("config", true))
    { // open in read-only
        state.network.owm_key = prefs.getString("owm_key", "");
        state.env.lat = prefs.getDouble("lat", 0.0);
        state.env.lon = prefs.getDouble("lon", 0.0);
        prefs.end();
        Serial.println(F("[DEBUG] State handmatig gevuld vanuit Preferences"));
    }
    else
    {
        Serial.println(F("[DEBUG] FOUT: Kon Preferences 'config' niet openen!"));
    }

    // Eerste berekeningen
    manageTimeFunctions(); // Bepaalt zon-tijden en vult state.env
    fetchWeather();        // Haalt OWM data en vult state.weather

    // Initialiseer Ticker voor de eerste keer
    updateTickerSegments();

    Serial.println(F("--- Systeem Start Voltooid ---"));

    // HELDERHEID TOEPASSEN (Hardware-fade naar de geladen waarde)
    updateDisplayBrightness(state.display.day_brightness);

    // // Preferences prefs;
    // prefs.begin("config", true); // 'true' betekent read-only

    // Serial.println("--- Inhoud van Preferences ---");
    // Serial.print("SSID: ");
    // Serial.println(prefs.getString("ssid", "NIET GEVONDEN"));
    // Serial.print("OWM Key: ");
    // Serial.println(prefs.getString("owm_key", "NIET GEVONDEN"));
    // Serial.print("Lat: ");
    // Serial.println(prefs.getDouble("lat", 0.0), 6);
    // Serial.print("Lon: ");
    // Serial.println(prefs.getDouble("lon", 0.0), 6);
    // prefs.end();
}

void loop()
{

    // In de normale loop
    static unsigned long lastTouchTick = 0;
    if (millis() - lastTouchTick > 30)
    {
        handleTouchLadder();
        manageStatusLed();
        lastTouchTick = millis();
    }

    // handleHardware();

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

    // --- BLOK 2: VISUEEL (TICKER & ANIMATIE) ---
    if (!state.display.is_animating && !state.network.is_updating)
    {
        renderTicker();
    }

    // --- BLOK 3: DE KLOK (ELKE SECONDE) ---
    static time_t last_now = 0;
    time_t now = time(nullptr);
    if (now != last_now)
    {
        last_now = now;
        updateClock();
    }

    // --- BLOK 4: SENSOREN & BEHEER (ELKE 10 SECONDEN) ---
    static unsigned long lastTenSecondTick = 0;
    if (millis() - lastTenSecondTick > 10000)
    {
        handleSensors();
        manageBrightness();
        manageTimeFunctions();
        // handleWiFiEco();
        manageServerTimeout();
        // Als we in de toekomst OTA willen, is dit ook een goed moment om even te checken
        // if (state.network.ota_enabled) { ArduinoOTA.handle(); }

        lastTenSecondTick = millis();
    }

    // --- BLOK 5: DATA UPDATES  ---
    manageWeatherUpdates();

    // --- BLOK 6: PANELEN & UI ---
    handleWiFiEco();
    manageDataPanels();
    handleServerControl();
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

    // --- DEBUG: Toon CPU temperatuur rechtsonder ---
    tft.setTextFont(1); 
    tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    tft.drawFloat(state.env.case_temp, 1, 5, 140); // Voor debug: Case temp

    // --- DEBUG: Toon vrije RAM in KB ---
    // tft.setTextFont(1); 
    // tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    // tft.drawNumber(ESP.getFreeHeap() / 1024, 5, 140); // Toont vrije KB's
}