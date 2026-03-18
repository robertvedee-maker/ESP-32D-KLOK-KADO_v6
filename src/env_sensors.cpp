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
    Preferences prefs; // Lokale instantie van Preferences
    
    float currentP = (state.env.press_local > 800.0f) ? state.env.press_local : state.env.pressure;
    time_t nu;
    time(&nu);

    static time_t lastSaveTime = 0;

    // 1. INITIALISATIE (Bij opstarten)
    if (lastSaveTime == 0) {
        prefs.begin("env_data", true);
        state.env.baro_ref_3h = prefs.getFloat("baro_ref", currentP);
        state.env.baro_trend = prefs.getInt("baro_trend", 0); // Laad ook de laatste trend!
        lastSaveTime = prefs.getLong("baro_time", 0);
        prefs.end();
    }

    // 2. DE 3-UURS UPDATE & TREND BEPALING
    // Alleen NU berekenen we de trend en leggen die vast voor de komende 3 uur
    if (nu - lastSaveTime > 10800 || lastSaveTime == 0) {
        if (nu > 1700000000) { 
            // Bereken de trend op basis van het AFGELOPEN blok
            float diff = currentP - state.env.baro_ref_3h;
            
            if (diff >2.0f)       state.env.baro_trend = 2; // Sterk stijgend
            else if (diff > 1.1f) state.env.baro_trend = 1; // Stijgend
            else if (diff < -2.0f)      state.env.baro_trend = -2; // Sterk dalend
            else if (diff < -1.1f)     state.env.baro_trend = -1; // Dalend
            else                      state.env.baro_trend = 0; // Stabiel

            // Sla alles op voor de volgende 3 uur
            prefs.begin("env_data", false);
            prefs.putFloat("baro_ref", currentP);
            prefs.putInt("baro_trend", state.env.baro_trend);
            prefs.putLong("baro_time", nu);
            prefs.end();

            state.env.baro_ref_3h = currentP;
            lastSaveTime = nu;
            Serial.printf("[BARO] Trend vastgelegd: %d (Diff: %.1f)\n", state.env.baro_trend, diff);
        }
    }
}

