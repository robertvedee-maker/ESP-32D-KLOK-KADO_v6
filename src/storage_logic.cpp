/* (c)2026 R van Dorland - Licensed under MIT License
 *
 * storage_logic.cpp - Alle functies die te maken hebben met het laden en opslaan van data in de flash van de ESP32.
 * 
 * Deze functies gebruiken de Preferences library om data op te slaan in de flash, zodat deze behouden blijft na een reboot.
 * We hebben aparte functies voor het laden en opslaan van verschillende soorten data, zoals netwerkconfiguratie, weerdata en display-instellingen.
 * Op deze manier kunnen we de code beter organiseren en de opslaglogica gescheiden houden van de rest van de app.
 */

#include "app_actions.h"

// #include <Preferences.h>
// #include "global_data.h"
// #include "storage_logic.h"


Preferences preferences;

// --- DE HOOFDKRAAN ---
void App::initStorage()
{
    Serial.printf("[STORAGE] Systeem laden...\n");
    // loadUserData();        // Laadt naam en geboortedatum
    App::loadNetworkConfig();   // Laadt SSID/Pass
    App::loadOMWConfig();       // Laadt OWM key en Lat/Lon
    App::loadDisplaySettings(); // Laadt Helderheid, Transitie, User en LED
    App::loadWeatherCache();    // Laadt de laatste weerdata (voor snelle weergave bij opstarten)
    Serial.printf("[STORAGE] Alles succesvol gesynchroniseerd.\n");
    // Debugging: Altijd handig om te zien of het gelukt is
    Serial.printf("[STORAGE] Geladen: Mode=%d, Naam=%s\n",
                  state.network.wifi_mode, state.user.name.c_str());
}

// --- DISPLAY & USER SETTINGS ---
void App::loadDisplaySettings()
{
    // Gebruik OVERAL "clock_cfg" voor consistentie
    preferences.begin("clock_cfg", true);

    state.user.name = preferences.getString("user_name", "");
    state.user.dob = preferences.getString("user_dob", "");

    state.display.day_brightness = preferences.getInt("br_day", state.display.day_brightness);
    state.display.night_brightness = preferences.getInt("br_night", state.display.night_brightness);
    state.display.transition_speed = preferences.getInt("tr_speed", state.display.transition_speed);
    state.display.transition_type = preferences.getInt("tr_type", state.display.transition_type);

    state.display.led_enabled = preferences.getBool("led_en", state.display.led_enabled);
    state.display.led_brightness = preferences.getInt("led_br", state.display.led_brightness);
    preferences.end();
}

void App::saveDisplaySettings()
{
    preferences.begin("clock_cfg", false);

    preferences.putString("user_name", state.user.name);
    preferences.putString("user_dob", state.user.dob);

    preferences.putInt("br_day", state.display.day_brightness);
    preferences.putInt("br_night", state.display.night_brightness);
    preferences.putInt("tr_speed", state.display.transition_speed);
    preferences.putInt("tr_type", state.display.transition_type);

    preferences.putBool("led_en", state.display.led_enabled);
    preferences.putInt("led_br", state.display.led_brightness);
    preferences.end();
}

// --- NETWERK ---
void App::loadNetworkConfig()
{
    preferences.begin("clock_cfg", true);
    // Als er niets in flash staat, wordt het "klok-kado"
    state.network.mdns = preferences.getString("mdns", "klok-kado");
    state.network.ssid = preferences.getString("ssid", "");
    state.network.pass = preferences.getString("pass", "");
    state.network.wifi_mode = preferences.getInt("wifi_mode", state.network.wifi_mode);
    preferences.end();
}

void App::saveNetworkConfig()
{
    preferences.begin("clock_cfg", false);
    preferences.putString("mdns", state.network.mdns);
    preferences.putString("ssid", state.network.ssid);
    preferences.putString("pass", state.network.pass);
    preferences.putInt("wifi_mode", state.network.wifi_mode);
    preferences.end();
}

// --- OWM & LOCATIE ---
void App::loadOMWConfig()
{
    preferences.begin("clock_cfg", true);
    state.network.owm_key = preferences.getString("owm_key", state.network.owm_key);
    state.env.lat = preferences.getDouble("lat", state.env.lat);
    state.env.lon = preferences.getDouble("lon", state.env.lon);
    preferences.end();
}

void App::saveOMWConfig()
{
    preferences.begin("clock_cfg", false);
    preferences.putString("owm_key", state.network.owm_key);
    preferences.putDouble("lat", state.env.lat);
    preferences.putDouble("lon", state.env.lon);
    preferences.end();
}

// --- WEER CACHE (In een eigen emmer om slijtage flash te spreiden) ---
void App::loadWeatherCache()
{
    preferences.begin("weer_cache", true);
    state.weather.temp = preferences.getFloat("last_t", 0.0);
    state.weather.current_icon = preferences.getInt("last_ico", 800);
    state.weather.description = preferences.getString("last_desc", "--");
    state.weather.last_update_epoch = preferences.getUInt("last_upd", 0);

    state.weather.today.sun_rise = preferences.getUInt("sun_r", 0);
    state.weather.today.sun_set = preferences.getUInt("sun_s", 0);
    state.weather.today.moon_rise = preferences.getUInt("moon_r", 0);
    state.weather.today.moon_set = preferences.getUInt("moon_s", 0);
    state.weather.today.moon_phase = preferences.getFloat("moon_p", 0.5);

    preferences.end();
}

void App::saveWeatherCache()
{
    preferences.begin("weer_cache", false);
    preferences.putFloat("last_t", state.weather.temp);
    preferences.putInt("last_ico", state.weather.current_icon);
    preferences.putString("last_desc", state.weather.description);
    preferences.putUInt("last_upd", state.weather.last_update_epoch);

    preferences.putUInt("sun_r", (uint32_t)state.weather.today.sun_rise);
    preferences.putUInt("sun_s", (uint32_t)state.weather.today.sun_set);
    preferences.putUInt("moon_r", (uint32_t)state.weather.today.moon_rise);
    preferences.putUInt("moon_s", (uint32_t)state.weather.today.moon_set);
    preferences.putFloat("moon_p", state.weather.today.moon_phase);

    preferences.end();
}
