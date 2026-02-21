#include <Preferences.h>
#include "global_data.h"
#include "storage_logic.h"

Preferences preferences;

// --- DE HOOFDKRAAN ---
void initStorage()
{
    Serial.println(F("[STORAGE] Systeem laden..."));
    // loadUserData();        // Laadt naam en geboortedatum
    loadNetworkConfig();   // Laadt SSID/Pass
    loadOMWConfig();       // Laadt OWM key en Lat/Lon
    loadDisplaySettings(); // Laadt Helderheid, Transitie, User en LED
    loadWeatherCache();    // Laadt de laatste weerdata (voor snelle weergave bij opstarten)
    Serial.println(F("[STORAGE] Alles succesvol gesynchroniseerd."));
    // Debugging: Altijd handig om te zien of het gelukt is
    Serial.printf("[STORAGE] Geladen: Mode=%d, Naam=%s\n",
                  state.network.wifi_mode, state.user.name.c_str());
}

// --- DISPLAY & USER SETTINGS ---
void loadDisplaySettings()
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

void saveDisplaySettings()
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
void loadNetworkConfig()
{
    preferences.begin("clock_cfg", true);
    // Als er niets in flash staat, wordt het "klok-kado"
    state.network.mdns = preferences.getString("mdns", "klok-kado");
    state.network.ssid = preferences.getString("ssid", "");
    state.network.pass = preferences.getString("pass", "");
    state.network.wifi_mode = preferences.getInt("wifi_mode", state.network.wifi_mode);
    preferences.end();
}

void saveNetworkConfig()
{
    preferences.begin("clock_cfg", false);
    preferences.putString("mdns", state.network.mdns);
    preferences.putString("ssid", state.network.ssid);
    preferences.putString("pass", state.network.pass);
    preferences.putInt("wifi_mode", state.network.wifi_mode);
    preferences.end();
}

// --- OWM & LOCATIE ---
void loadOMWConfig()
{
    preferences.begin("clock_cfg", true);
    state.network.owm_key = preferences.getString("owm_key", state.network.owm_key);
    state.env.lat = preferences.getDouble("lat", state.env.lat);
    state.env.lon = preferences.getDouble("lon", state.env.lon);
    preferences.end();
}

void saveOMWConfig()
{
    preferences.begin("clock_cfg", false);
    preferences.putString("owm_key", state.network.owm_key);
    preferences.putDouble("lat", state.env.lat);
    preferences.putDouble("lon", state.env.lon);
    preferences.end();
}

// --- WEER CACHE (In een eigen emmer om slijtage flash te spreiden) ---
void loadWeatherCache()
{
    preferences.begin("weer_cache", true);
    state.weather.temp = preferences.getFloat("last_t", 0.0);
    state.weather.current_icon = preferences.getInt("last_ico", 800);
    state.weather.description = preferences.getString("last_desc", "--");
    state.weather.last_update_epoch = preferences.getUInt("last_upd", 0);
    preferences.end();
}

void saveWeatherCache()
{
    preferences.begin("weer_cache", false);
    preferences.putFloat("last_t", state.weather.temp);
    preferences.putInt("last_ico", state.weather.current_icon);
    preferences.putString("last_desc", state.weather.description);
    preferences.putUInt("last_upd", state.weather.last_update_epoch);
    preferences.end();
}
