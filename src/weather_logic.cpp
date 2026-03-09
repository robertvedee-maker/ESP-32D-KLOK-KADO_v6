/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Weather logic: OpenWeatherMap API integratie
 */

#include "weather_logic.h"
#include "display_logic.h"
#include "global_data.h"
#include "helpers.h"
#include "secret.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
// #include <WiFiClientSecure.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

// forward declarations
bool fetchWeather(bool checkOnly);
void manageWeatherUpdates();
void updateTickerSegments();
String urlEncode(String str);
void setupWiFi();
void disableWiFi();
// void deactivateWiFiAndServer();
void powerDownWiFi();

void manageWeatherUpdates()
{
    static unsigned long lastMinuteCheck = 0;
    unsigned long nuMillis = millis();

    // 1. DE HARDE POORT: Geen uitzonderingen! We kijken ELKE 60 seconden. Punt.
    if (nuMillis - lastMinuteCheck < 60000)
    {
        // Serial.println(F("[WEATHER-MGMT] Nog te vroeg voor de volgende check..."));
        return;
    }

    // 2. DE GRENDEL: Zodra we hier voorbij zijn, gaat de poort direct op slot voor 60 sec.
    lastMinuteCheck = nuMillis;

    // 3. De Beslissing: Moeten we updaten?
    // We updaten als de vlag TRUE is (koude start) OF als fetchWeather(true) zegt dat het tijd is.
    bool moetNuUpdaten = Config::forceFirstWeatherUpdate || fetchWeather(true);

    if (!state.network.wifi_enabled && state.network.wifi_mode == 2)
        return; // WiFi is uit, en we zitten in ECO/Nacht modus, dus we respecteren dat en doen niks.

    // 4. DE UITVOERING
    if (moetNuUpdaten)
    {
        Serial.println(F("[WEATHER-MGMT] Start update proces..."));

        // WiFi check
        if (WiFi.status() != WL_CONNECTED)
        {
            setupWiFi();
            unsigned long startWait = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startWait < 5000)
            {
                vTaskDelay(pdMS_TO_TICKS(100)); // Wacht netjes in Core 0
                // delay(50);
                // yield();
            }
        }

        // De Fetch
        if (WiFi.status() == WL_CONNECTED)
        {
            if (fetchWeather(false))
            {
                // Update gelukt: Vlag naar false, vanaf nu geldt de 30-min regel
                Config::forceFirstWeatherUpdate = false;
                Serial.println(F("[WEATHER-MGMT] Update voltooid. Volgende over 30 min."));
            }
            else
            {
                Serial.println(F("[WEATHER-MGMT] Fetch mislukt, probeer over 1 minuut opnieuw."));
            }
        }
        // On-demand: WiFi weer uit na gebruik
        if (state.network.wifi_mode == 1 && !state.network.web_server_active)
        {
            // disableWiFi();
            // deactivateWiFiAndServer();
            powerDownWiFi();

        }

        state.network.is_updating = false;
    }
}

bool fetchWeather(bool checkOnly)
{
    // 1. VOORWAARDEN CHECK
    if (state.network.owm_key.length() < 10)
    {
        return false;
    }

    // 2. TIJD & RATE LIMIT CHECK
    time_t nu = time(nullptr);
    Preferences prefs;
    prefs.begin("config", true);
    time_t laatsteUpdate = (time_t)prefs.getULong("last_owm_epoch", 0);
    prefs.end();

    const unsigned long interval = 30 * 60; // 30 minuten

    // DE LOGICA:
    // We laten de update door als:
    // A. Het een FORCE update is (koude start)
    // B. OF de tijd nog niet gesynchroniseerd is (nu < 100000) -> Eerste run na boot
    // C. OF de verstreken tijd groter is dan het interval

    bool tijdOm = (laatsteUpdate == 0) || (nu - laatsteUpdate >= interval);

    // Als de tijd nog op 1970 staat (net na boot),
    // kunnen we de 'nu - laatsteUpdate' niet vertrouwen.
    // In dat geval laten we hem door als het de eerste keer is.
    if (nu < 1000000)
    {
        tijdOm = true;
    }

    if (!Config::forceFirstWeatherUpdate && !tijdOm)
    {
        // Alleen loggen als we niet alleen aan het checken zijn
        if (!checkOnly)
        {
            Serial.println(F("[OWM] Te vroeg, we wachten op slot..."));
        }
        return false;
    }

    if (checkOnly)
        return true;

    state.network.is_updating = true;

    Serial.println(F("[WEATHER] API call wordt aangesproken!..."));

    // 2. HTTP CONNECTIE
    WiFiClient client;
    HTTPClient http;
    String url = String(Config::owm_base_url);
    url += "lat=" + String(state.env.lat, 4);
    url += "&lon=" + String(state.env.lon, 4);
    url += "&lang=nl&units=metric&exclude=minutely,hourly";
    url += "&appid=" + state.network.owm_key;

    if (!http.begin(client, url))
    {
        state.network.is_updating = false;
        prefs.end();
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        http.end();
        state.network.is_updating = false;
        prefs.end();
        return false;
    }

    // 3. JSON PARSING
    JsonDocument filter;
    filter["current"] = true;
    filter["daily"] = true;
    filter["alerts"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end(); // We kunnen de verbinding nu al sluiten

    if (error)
    {
        Serial.print(F("[WEATHER] JSON Fout: "));
        Serial.println(error.c_str());
        state.network.is_updating = false;
        prefs.end();
        return false;
    }

    // 4. DATA MAPPING
    auto cur = doc["current"];
    if (!cur["temp"].is<float>())
    {
        Serial.println(F("[WEATHER] Geen geldige temperatuur in JSON."));
        state.network.is_updating = false;
        prefs.end();
        return false;
    }

    // --- HUIDIG WEER ---
    state.weather.temp = cur["temp"] | 0.0f;
    state.weather.feels_like = cur["feels_like"] | 0.0f;
    state.weather.visibility = cur["visibility"] | 0;
    state.weather.clouds = cur["clouds"] | 0;
    state.weather.uvi = cur["uvi"] | 0.0f;
    state.weather.pressure = cur["pressure"] | 0.0f;
    state.weather.humidity = cur["humidity"] | 0.0f;
    state.weather.dew_point = cur["dew_point"] | 0.0f;
    state.weather.wind_speed = cur["wind_speed"] | 0.0f;
    state.weather.wind_deg = cur["wind_deg"] | 0;
    state.weather.wind_gust = cur["wind_gust"] | 0.0f;
    state.weather.description = cur["weather"][0]["description"] | "--";
    state.weather.current_icon = cur["weather"][0]["id"] | 800;

    state.weather.last_update_epoch = time(nullptr);
    saveWeatherCache(); // Snel opslaan in NVS

    // --- ALERTS ---
    if (doc["alerts"].size() > 0)
    {
        state.env.is_alert_active = true;
        // We pakken de Engelse tekst direct uit de OWM stream
        state.weather.alert_text = doc["alerts"][0]["description"] | "Weather Alert Active";
        state.weather.alert_start = doc["alerts"][0]["start"] | 0;
        state.weather.alert_end = doc["alerts"][0]["end"] | 0;
    }
    else
    {
        state.env.is_alert_active = false;
        state.weather.alert_text = "";
    }

    // --- GEGEVENS VOOR VANDAAG (Dag 0 uit de forecast sectie) ---
    auto today = doc["daily"][0]; // <--- Belangrijk: Index 0 is vandaag!

    // En dan de waarden overzetten naar je state:
    strlcpy(state.weather.today.summary, today["summary"] | "--", sizeof(state.weather.today.summary));
    state.weather.today.temp_min = today["temp"]["min"] | 0.0f;
    state.weather.today.temp_max = today["temp"]["max"] | 0.0f;
    state.weather.today.temp_night = today["temp"]["night"] | 0.0f;
    state.weather.today.temp_eve = today["temp"]["eve"] | 0.0f;
    state.weather.today.temp_morn = today["temp"]["morn"] | 0.0f;
    state.weather.today.sun_rise = today["sunrise"] | 0;
    state.weather.today.sun_set = today["sunset"] | 0;
    state.weather.today.moon_phase = today["moon_phase"] | 0.0f;
    state.weather.today.moon_rise = today["moonrise"] | 0;
    state.weather.today.moon_set = today["moonset"] | 0;

    // --- FORECAST (3 DAGEN) ---
    for (int i = 0; i < 3; i++)
    {
        auto daily = doc["daily"][i + 1];
        auto &f = state.weather.forecast[i];
        f.dt = daily["dt"] | 0;
        f.temp_min = daily["temp"]["min"] | 0.0f;
        f.temp_max = daily["temp"]["max"] | 0.0f;
        f.icon_id = daily["weather"][0]["id"] | 800;
        strlcpy(f.description, daily["weather"][0]["description"] | "--", sizeof(f.description));
    }

    // 6. AFHANDELING & OPSLAAN
    state.weather.last_update_epoch = nu;
    state.weather.data_is_fresh = true;
    saveWeatherCache();
    updateTickerSegments();

    // Sla de echte Unix-tijd op (overleeft reboots en hitte-crashes)
    prefs.begin("config", false);
    prefs.putULong("last_owm_epoch", (uint32_t)nu);
    prefs.end();

    state.network.is_updating = false;
    Config::forceFirstWeatherUpdate = false; // Reset de forceer-vlag na succes
    return true;
}
