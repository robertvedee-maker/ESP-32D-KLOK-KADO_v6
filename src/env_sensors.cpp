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

// Instanties
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// // We maken een globale 'handle' aan die alleen in dit bestand zichtbaar is
// static temperature_sensor_handle_t temp_sensor = NULL;
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(Config::pin_OneWire); // GPIO32 voor DS18B20

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);



// // Lokale buffer voor het hoogte-filter (10 metingen)
static float hoogte_buffer[10];
static int buffer_idx = 0;
static bool buffer_vol = false;

// Status flags om crashes te voorkomen
static bool aht_ok = false;
static bool bmp_ok = false;
static bool ds18b20_ok = false;

bool setupSensors()
{
    // 1. Start I2C op de juiste pinnen
    if (!Wire.begin(I2C_SDA, I2C_SCL))
    {
        Serial.println("[SENSOR] I2C Bus Fout!");
        return false;
    }

    // 2. AHT20 Initialisatie
    if (aht.begin())
    {
        aht_ok = true;
        Serial.println("[SENSOR] AHT20 OK");
    }
    else
    {
        aht_ok = false;
        Serial.println("[SENSOR] AHT20 niet gevonden!");
    }

    // 3. BMP280 Initialisatie (check beide adressen)
    if (bmp.begin(0x76) || bmp.begin(0x77))
    {
        bmp_ok = true;
        Serial.println("[SENSOR] BMP280 OK");

        // Optioneel: BMP instellingen voor betere stabiliteit
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X1, // Temperatuur
                        Adafruit_BMP280::SAMPLING_X2, // Druk
                        Adafruit_BMP280::FILTER_OFF,
                        Adafruit_BMP280::STANDBY_MS_1000);
    }
    else
    {
        bmp_ok = false;
        Serial.println("[SENSOR] BMP280 niet gevonden!");
    }

    // 4. DS18B20 Initialisatie
    sensors.begin();
    if (sensors.getDeviceCount() > 0)
    {
        sensors.setWaitForConversion(false); // CRUCIAAL: Dit zet de asynchrone modus aan
        sensors.requestTemperatures();       // Geef alvast de allereerste opdracht
        ds18b20_ok = true;
        Serial.println("[SENSOR] DS18B20 OK");
    }
    else
    {
        ds18b20_ok = false;
        Serial.println("[SENSOR] DS18B20 niet gevonden!");
    }

    return (aht_ok || bmp_ok || ds18b20_ok); // True als er tenminste één sensor werkt
}

void handleSensors()
{
   // 0. DS18B20: Interne Behuizing Temperatuur (Asynchroon)
    if (ds18b20_ok)
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
    if (aht_ok)
    {
        sensors_event_t humidity, temp;
        if (aht.getEvent(&humidity, &temp))
        {
            state.env.temp_local = temp.temperature;
            state.env.hum_local = humidity.relative_humidity;
        }
    }

    // 2. BMP280: Druk en Hoogte
    if (bmp_ok)
    {
        // FORCEER één meting
        bmp.takeForcedMeasurement();

        // Haal de waarden op (deze gebruiken nu de data van de forced measurement)
        float raw_pressure = bmp.readPressure() / 100.0F;
        state.env.press_local = raw_pressure;

        // // 1. Directe meting voor clock display
        // state.env.press_local = bmp.readPressure() / 100.0F; // hPa

        // Bereken hoogte (gebruik 1013.25 als referentie)
        float current_alt = bmp.readAltitude(1013.25);
        state.env.altitude_local = current_alt;

        // Update de string die de Ticker gebruikt (zonder decimalen + "m")
        state.weather.height_str = String((int)(current_alt + 0.5)) + "m";

        // 1. Statics voor stabiliteit
        static float laatste_valide_raw = 0;
        static int getoonde_int_hoogte = -999;
        // static unsigned long start_tijd = millis();
        // bool opgewarmd = (millis() - start_tijd > 45000); // 45 sec opwarmtijd

        // 2. Ruwe meting
        float ref_druk = (state.weather.pressure > 800) ? state.weather.pressure : 1013.25;
        float rauwe_hoogte = bmp.readAltitude(ref_druk);

        // 3. Delta-check (Glitch filter)
        float delta = abs(rauwe_hoogte - laatste_valide_raw);
        if (/*opgewarmd &&*/ laatste_valide_raw != 0 && delta > 80.0)
        {
            // Sprong van > 80m in 1 sec negeren we (Glitch)
            return;
        }
        laatste_valide_raw = rauwe_hoogte;

        // 4. Moving Average Buffer
        hoogte_buffer[buffer_idx] = rauwe_hoogte;
        buffer_idx = (buffer_idx + 1) % 10;
        if (buffer_idx == 0)
            buffer_vol = true;

        float som = 0;
        int aantal = buffer_vol ? 10 : buffer_idx;
        for (int i = 0; i < aantal; i++)
            som += hoogte_buffer[i];
        state.weather.stable_height = som / aantal;

        // 5. DEAD_BAND Logica voor de Ticker
        int afgeronde_hoogte = (int)(state.weather.stable_height + 0.5);

        // Update de string alleen als:
        // - We zijn opgewarmd EN de buffer vol is
        // - EN de wijziging >= 2 meter is (Deadband)
        if (/*opgewarmd &&*/ buffer_vol)
        {
            if (abs(afgeronde_hoogte - getoonde_int_hoogte) >= 2 || getoonde_int_hoogte == -999)
            {
                getoonde_int_hoogte = afgeronde_hoogte;

                // Zet in de state voor de Ticker
                state.weather.height_str = String(getoonde_int_hoogte);
            }
        }
        else
        {
            // Tijdens opwarmen houden we de Ticker-check op "0"
            state.weather.height_str = "0";
        }
    }
    // Serial.printf("[SENSOR] Temp: %.1f C, Hum: %.1f %%, Press: %.1f hPa, Alt: %.1f m\n",
    //               state.env.temp_local, state.env.hum_local, state.env.press_local, state.env.altitude_local);
}

// void updateSensors()
// {
//     // 1. AHT20 Uitlezen
//     sensors_event_t humidity, temp;
//     if (aht.getEvent(&humidity, &temp))
//     {
//         // Sla op in de S.P.O.T. emmers
//         state.env.temp_local = temp.temperature;
//         state.env.hum_local = humidity.relative_humidity;
//     }

//     // 2. BMP280 Uitlezen
//     if (bmp_ok)
//     {
//         state.env.press_local = bmp.readPressure() / 100.0F; // hPa

//         // Bereken hoogte (gebruik 1013.25 als referentie)
//         float current_alt = bmp.readAltitude(1013.25);
//         state.env.altitude_local = current_alt;

//         // Update de string die de Ticker gebruikt (zonder decimalen + "m")
//         state.weather.height_str = String((int)(current_alt + 0.5)) + "m";
//     }

//     Serial.printf("[SENSOR] Temp: %.1f C, Hum: %.1f %%, Press: %.1f hPa, Alt: %.1f m\n",
//                   state.env.temp_local, state.env.hum_local, state.env.press_local, state.env.altitude_local);
// }

// bool setupInternalTempSensor() {
//     temperature_sensor_config_t temp_sensor_config = {
//         .range_min = 10,
//         .range_max = 80,
//     };
//     // Initialiseer de sensor
//     ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
//     // Start de sensor
//     ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
//     return true;
// }

// void updateInternalSensors() {
//     float tsens_out;
//     if (temp_sensor != NULL) {
//         // Lees de waarde in Celsius
//         ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_out));
//         state.env.cpu_temp = tsens_out;
//     }
// }
