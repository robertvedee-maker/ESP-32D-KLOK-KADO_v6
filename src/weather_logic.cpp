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

void updateTickerSegments();
String urlEncode(String str);
void setupWiFi();


void manageWeatherUpdates() {
    static unsigned long lastMinuteCheck = 0;
    unsigned long nuMillis = millis();

    // 1. DE HARDE POORT: Geen uitzonderingen! We kijken ELKE 60 seconden. Punt.
    if (nuMillis - lastMinuteCheck < 60000) {
        return; 
    }
    
    // 2. DE GRENDEL: Zodra we hier voorbij zijn, gaat de poort direct op slot voor 60 sec.
    lastMinuteCheck = nuMillis;

    // 3. De Beslissing: Moeten we updaten?
    // We updaten als de vlag TRUE is (koude start) OF als fetchWeather(true) zegt dat het tijd is.
    bool moetNuUpdaten = Config::forceFirstWeatherUpdate || fetchWeather(true);

    // 4. DE UITVOERING
    if (moetNuUpdaten) {
        // Serial.println(F("[WEATHER] Start update proces..."));
        
        // WiFi check
        if (WiFi.status() != WL_CONNECTED) {
            setupWiFi(); 
            unsigned long startWait = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startWait < 5000) {
                delay(50); yield();
            }
        }

        // De Fetch
        if (WiFi.status() == WL_CONNECTED) {
            if (fetchWeather(false)) {
                // Update gelukt: Vlag naar false, vanaf nu geldt de 30-min regel
                Config::forceFirstWeatherUpdate = false;
                Serial.println(F("[WEATHER-MGMT] Update voltooid. Volgende over 30 min."));
            } else {
                Serial.println(F("[WEATHER-MGMT] Fetch mislukt, probeer over 1 minuut opnieuw."));
            }
        }
    } 
}


bool fetchWeather(bool checkOnly)
{
    // 1. VOORWAARDEN CHECK
    if (state.network.owm_key.length() < 10)
    {
        Serial.println(F("[WEATHER] Afgebroken: API Key te kort of leeg."));
        state.network.is_updating = false;
        return false;
    }

    // // 1. RATE LIMIT CHECK (30 minuten)
    Preferences prefs;
    prefs.begin("config", false);

    unsigned long nu = millis() / 1000; // Tijd in seconden
    unsigned long laatsteUpdate = prefs.getULong("last_owm", 0);
    const unsigned long interval = 30 * 60; // 1800 seconden (30 min)

    // DE RECHTE LIJN:
    // We blokkeren de update ALLEEN als:
    // (Het GEEN force-update is) EN (De tijd nog niet om is)
    if (!Config::forceFirstWeatherUpdate) 
    {
        if (laatsteUpdate > 0 && (nu - laatsteUpdate < interval) && nu > laatsteUpdate)
        {
            if (!checkOnly) Serial.println(F("[OWM] Te vroeg, we wachten op slot..."));
            prefs.end();
            return false; 
        }
    }

    // Als we alleen kwamen om te checken, stoppen we hier
    if (checkOnly) {
        prefs.end();
        return true;
    }
    
    // We gaan updaten, zet de vlag omhoog
    state.network.is_updating = true;

    // 2. HTTP CONNECTIE
    WiFiClient client;
    HTTPClient http;
    String url = String(Config::owm_base_url);
    url += "lat=" + String(state.env.lat, 4);
    url += "&lon=" + String(state.env.lon, 4);
    url += "&lang=nl&units=metric&exclude=minutely,hourly";
    url += "&appid=" + state.network.owm_key;

    if (!http.begin(client, url)) {
        state.network.is_updating = false;
        prefs.end();
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
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
    state.weather.description = cur["weather"][0]["description"] | "--";
    state.weather.current_icon = cur["weather"][0]["id"] | 800;

    state.weather.last_update_epoch = time(nullptr);
    saveWeatherCache(); // Snel opslaan in NVS

    // --- ALERTS ---
     if (doc["alerts"].size() > 0) {
        state.env.is_alert_active = true;
        // We pakken de Engelse tekst direct uit de OWM stream
        state.weather.alert_text = doc["alerts"][0]["description"] | "Weather Alert Active";
        // Optioneel: zet het in hoofdletters voor een 'waarschuwing' look
        state.weather.alert_text.toUpperCase();
    } else {
        state.env.is_alert_active = false;
        state.weather.alert_text = "";
    }

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
    state.weather.last_update_epoch = time(nullptr);
    saveWeatherCache(); 
    updateTickerSegments();
    
    // Sla de huidige tijd op in de Preferences
    prefs.putULong("last_owm", nu); 
    prefs.end();
    
    state.network.is_updating = false;
    Serial.println(F("[WEATHER] OWM 3.0 Update Succesvol."));
    return true;
}

// String translateToNL(String englishText) {
//     if (englishText == "" || englishText == "null") return "";

//     HTTPClient http;
    
//     // We gebruiken een mirror die nog HTTP (poort 80) toestaat. 
//     // DIT VREET GEEN RAM (geen SSL nodig).
//     String url = "http://translate.argosopentech.com";

//     if (http.begin(url)) {
//         http.addHeader("Content-Type", "application/json");

//         // Handmatige JSON opbouw om geheugen te sparen
//         String httpRequestData = "{\"q\":\"" + englishText + "\",\"source\":\"en\",\"target\":\"nl\",\"format\":\"text\"}";

//         int httpResponseCode = http.POST(httpRequestData);
//         String translatedText = englishText;

//         if (httpResponseCode == 200) {
//             String response = http.getString();
//             JsonDocument doc;
//             deserializeJson(doc, response);
//             if (doc["translatedText"].is<String>()) {
//                 translatedText = doc["translatedText"].as<String>();
//                 Serial.println("Vertaald via HTTP (RAM-safe)!");
//             }
//         }
//         http.end();
//         return translatedText;
//     }
//     return englishText;
// }


// // Hulpmiddel om vreemde tekens in de tekst veilig te versturen (spaties, leestekens etc.)
// String urlEncode(String str)
// {
//     String encodedString = "";
//     char c;
//     char code0;
//     char code1;
//     for (int i = 0; i < str.length(); i++)
//     {
//         c = str.charAt(i);
//         if (isalnum(c))
//         {
//             encodedString += c;
//         }
//         else
//         {
//             code1 = (c & 0xf) + '0';
//             if ((c & 0xf) > 9)
//                 code1 = (c & 0xf) - 10 + 'A';
//             c = (c >> 4) & 0xf;
//             code0 = c + '0';
//             if (c > 9)
//                 code0 = c - 10 + 'A';
//             encodedString += '%';
//             encodedString += code0;
//             encodedString += code1;
//         }
//     }
//     return encodedString;
// }

// String translateToNL(String englishText)
// {
//     if (englishText == "")
//         return "";

//     HTTPClient http;
//     // Gebruik de publieke endpoint
//     String url = "https://de.libretranslate.com/translate";

//     http.begin(url);
//     http.addHeader("Content-Type", "application/json");

//     // 1. Bouw de JSON aanvraag (v7)
//     JsonDocument requestDoc;
//     requestDoc["q"] = englishText;
//     requestDoc["source"] = "en";
//     requestDoc["target"] = "nl";
//     requestDoc["format"] = "text";

//     String httpRequestData;
//     serializeJson(requestDoc, httpRequestData);

//     int httpResponseCode = http.POST(httpRequestData);
//     String translatedText = englishText; // Fallback naar origineel bij fout

//     if (httpResponseCode == HTTP_CODE_OK)
//     {
//         String response = http.getString();
//         JsonDocument responseDoc;
//         DeserializationError error = deserializeJson(responseDoc, response);

//         if (!error && responseDoc["translatedText"].is<String>())
//         {
//             translatedText = responseDoc["translatedText"].as<String>();
//             Serial.println("Vertaling succesvol ontvangen.");
//         }
//     }
//     else
//     {
//         Serial.printf("Vertaling mislukt, code: %d\n", httpResponseCode);
//     }

//     http.end();
//     return translatedText;
// }
