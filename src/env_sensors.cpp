/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Environmental sensors: Temperatuur, Luchtvochtigheid, Druk
 */

#include "env_sensors.h"
#include "global_data.h"
#include "helpers.h"
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

// Instanties
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
OneWire oneWire(Config::pin_OneWire); // GPIO32 voor DS18B20
DallasTemperature sensors(&oneWire);
extern Preferences preferences;


// --- Sensor Setup en Handling ---
bool setupSensors()
{
    // 1. Start I2C op de juiste pinnen
    if (!Wire.begin(I2C_SDA, I2C_SCL))
    {
        Serial.printf("[SENSOR] I2C Bus Fout!\n");
        return false;
    }

    // 2. AHT20 Initialisatie
    if (aht.begin())
    {
        state.env.aht_ok = true;
        Serial.printf("[SENSOR] AHT20 OK\n");
    }
    else
    {
        state.env.aht_ok = false;
        Serial.printf("[SENSOR] AHT20 niet gevonden!\n");
    }

    // 3. BMP280 Initialisatie (check beide adressen)
    if (bmp.begin(0x76) || bmp.begin(0x77))
    {
        state.env.bmp_ok = true;
        Serial.printf("[SENSOR] BMP280 OK\n");

        // Optioneel: BMP instellingen voor betere stabiliteit
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X1, // Temperatuur
                        Adafruit_BMP280::SAMPLING_X2, // Druk
                        Adafruit_BMP280::FILTER_OFF,
                        Adafruit_BMP280::STANDBY_MS_1000);
    }
    else
    {
        state.env.bmp_ok = false;
        Serial.printf("[SENSOR] BMP280 niet gevonden!\n");
    }

    // 4. DS18B20 Initialisatie
    sensors.begin();
    if (sensors.getDeviceCount() > 0)
    {
        sensors.setWaitForConversion(false); // CRUCIAAL: Dit zet de asynchrone modus aan
        sensors.requestTemperatures();       // Geef alvast de allereerste opdracht
        state.env.ds18b20_ok = true;
        Serial.printf("[SENSOR] DS18B20 OK\n");
    }
    else
    {
        state.env.ds18b20_ok = false;
        Serial.printf("[SENSOR] DS18B20 niet gevonden!\n");
    }

    return (state.env.aht_ok || state.env.bmp_ok || state.env.ds18b20_ok); // True als er tenminste één sensor werkt
}

// --- Sensor Data Handling (elke 10 sec) ---
void handleSensors()
{
   // 0. DS18B20: Interne Behuizing Temperatuur (Asynchroon)
    if (state.env.ds18b20_ok)
    {
        // Haal de waarde op die de VORIGE ronde (10 sec geleden) is aangevraagd
        float current_case_temp = sensors.getTempCByIndex(0);

        // Alleen updaten als de meting valide is (-127.0 is fout)
        if (current_case_temp != DEVICE_DISCONNECTED_C)
        {
            state.env.case_temp = current_case_temp;
        }

        // Geef DIRECT de opdracht voor de VOLGENDE meting. 
        // Dankzij 'setWaitForConversion(false)' in setup duurt dit < 1ms.
        sensors.requestTemperatures();
    }


    // 1. AHT20: Temperatuur en Luchtvochtigheid
    if (state.env.aht_ok)
    {
        sensors_event_t humidity, temp;
        if (aht.getEvent(&humidity, &temp))
        {
            state.env.raw_temp_local = temp.temperature;
            state.env.hum_local = humidity.relative_humidity;
        }
    }

    if (state.env.aht_ok && state.env.ds18b20_ok) {
        // We hebben vastgesteld: AHT is 5 graden te hoog t.o.v. de buitenwereld
        // als de case_temp rond de 30 graden zit.
        
        float offset = 5.0; 
        
        // We passen een dynamische correctie toe:
        // Als de behuizing op kamertemperatuur is (bijv. na koude start), corrigeren we niets.
        // Naarmate de behuizing opwarmt, passen we de volledige offset toe.
        
        float opwarmFactor = (state.env.case_temp - 20.0) / 8.0; 
        opwarmFactor = constrain(opwarmFactor, 0.0, 2.0); // Blijf tussen 0 en 2
        
        state.env.temp_local = state.env.raw_temp_local - (offset * opwarmFactor);
        // Serial.printf("[SENSOR] AHT20 Temperatuur gecorrigeerd met %.2f graden (Factor: %.2f)\n", offset * opwarmFactor, opwarmFactor);
    }

    // 2. BMP280: Druk en Hoogte
    if (state.env.bmp_ok)
    {
        // // 1. Directe meting voor clock display
        state.env.press_local = bmp.readPressure() / 100.0F; // hPa
    }

}

//  --- BAROMETER TREND LOGICA ---
void updateBaroTrend() {
    float currentP = (state.env.press_local > 800.0f) ? state.env.press_local : state.env.pressure;
    time_t nu;
    time(&nu); // Haal de huidige Epoch tijd op

    static time_t lastSaveTime = 0;

    // 1. INITIALISATIE (Alleen bij start)
    if (lastSaveTime == 0) {
        preferences.begin("env_data", true);
        state.env.baro_ref_3h = preferences.getFloat("baro_ref", currentP);
        lastSaveTime = preferences.getLong("baro_time", 0); // Wanneer hebben we voor het laatst opgeslagen?
        preferences.end();
    }

    // 2. TREND REKENEN
    float diff = currentP - state.env.baro_ref_3h;
    if (diff > 1.1f)       state.env.baro_trend = 1;
    else if (diff < -1.1f) state.env.baro_trend = -1;
    else                   state.env.baro_trend = 0;

    // 3. DE SLIMME TIMER (Check op Epoch verschil)
    // 3 uur = 10800 seconden
    if (nu - lastSaveTime > 10800 || lastSaveTime == 0) {
        if (nu > 1700000000) { // Alleen opslaan als we een geldige tijd hebben (na jaar 2023)
            preferences.begin("env_data", false);
            preferences.putFloat("baro_ref", currentP);
            preferences.putLong("baro_time", nu); // Sla de ECHTE TIJD op
            preferences.end();

            state.env.baro_ref_3h = currentP;
            lastSaveTime = nu;
            Serial.println(F("[BARO] 3-uurs referentie ververst op basis van echte tijd."));
        }
    }
}


// void updateBaroTrend() {
//     // 1. Pak de LOKALE druk (BMP280). Is die 0? Gebruik dan de algemene druk.
//     float currentP = (state.env.press_local > 800.0f) ? state.env.press_local : state.env.pressure;
    
//     static float lastRef = 0;
//     static unsigned long lastSave = 0;

//     // 2. INITIALISATIE (Alleen bij de allereerste start)
//     if (lastRef < 100.0f) { 
//         preferences.begin("env_data", true);
//         lastRef = preferences.getFloat("baro_ref", currentP);
//         preferences.end();
//         state.env.baro_ref_3h = lastRef;
//         lastSave = millis();
//         Serial.printf("[BARO] Start-referentie: %.1f hPa (Lokaal: %.1f)\n", lastRef, state.env.press_local);
//     }

//     // 3. TREND BEPALEN
//     float diff = currentP - state.env.baro_ref_3h;
    
//     if (diff > 1.1f)       state.env.baro_trend = 1;  // Stijgend
//     else if (diff < -1.1f) state.env.baro_trend = -1; // Dalend
//     else                   state.env.baro_trend = 0;  // Stabiel

//     // 4. PERIODIEKE OPSLAG (Eens per 3 uur)
//     if (millis() - lastSave > 3 * 3600000UL) {
//         preferences.begin("env_data", false);
//         preferences.putFloat("baro_ref", currentP);
//         preferences.end();
        
//         state.env.baro_ref_3h = currentP;
//         lastSave = millis();
//         Serial.println(F("[BARO] 3-uurs ijkpunt vernieuwd met lokale BMP data."));
//     }
//     Serial.printf("[BARO] Nu: %.1f, Ref: %.1f, Diff: %.1f, Trend: %d\n", currentP, state.env.baro_ref_3h, diff, state.env.baro_trend);
// }

// void updateBaroTrend() {
//     static float lastRef = 0;
//     static unsigned long lastSave = 0;
//     float currentP = state.env.pressure; // BMP280 waarde

//     // 1. INITIALISATIE (Alleen bij de allereerste start na stroom/reset)
//     if (lastRef < 100.0f) { 
//         preferences.begin("env_data", true);
//         // Haal de oude referentie op. Bestaat die niet? Gebruik dan de huidige druk.
//         lastRef = preferences.getFloat("baro_ref", currentP);
//         preferences.end();
//         state.env.baro_ref_3h = lastRef;
//         lastSave = millis(); // Start de 3-uurs timer vanaf nu
//         Serial.printf("[BARO] Referentie geladen: %.1f hPa\n", lastRef);
//     }

//     // 2. TREND BEPALEN (Elke 10 seconden als deze functie wordt aangeroepen)
//     float diff = currentP - state.env.baro_ref_3h;
    
//     // Drempel: 1.1 hPa verschil over 3 uur (meteorologische standaard)
//     if (diff > 1.1f)       state.env.baro_trend = 1;  // Stijgend (Beter weer)
//     else if (diff < -1.1f) state.env.baro_trend = -1; // Dalend (Slechter weer)
//     else                   state.env.baro_trend = 0;  // Stabiel

//     // 3. PERIODIEKE OPSLAG (Eens per 3 uur)
//     if (millis() - lastSave > 3 * 3600000UL) {
//         preferences.begin("env_data", false);
//         preferences.putFloat("baro_ref", currentP);
//         preferences.end();
        
//         state.env.baro_ref_3h = currentP; // Het nieuwe ijkpunt voor de komende 3 uur
//         lastSave = millis();
//         Serial.printf("[BARO] Nieuw 3-uurs ijkpunt opgeslagen in NVS.\n");
//     }
//     Serial.printf("[BARO] Nu: %.1f, Ref: %.1f, Diff: %.1f, Trend: %d\n", currentP, state.env.baro_ref_3h, diff, state.env.baro_trend);
// }
