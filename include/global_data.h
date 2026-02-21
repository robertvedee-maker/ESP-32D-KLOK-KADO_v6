#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h" // We gebruiken Config:: voor de defaults

// --- TYPES & ENUMS ---
struct TickerSegment {
    String text;
    uint16_t color;
    int width;
};

struct UserData {
    String name = "";
    String dob = ""; // YYYY-MM-DD
};

struct DailyForecast {
    long dt = 0;
    float temp_min = 0.0;
    float temp_max = 0.0;
    int icon_id = 800;
    float moon_phase = 0.0;
    char description[32] = "";
};

// --- SUBSYSTEM STATES ---

struct WeatherData {
    uint32_t last_update_epoch = 0;
    float temp = 0.0, feels_like = 0.0, humidity = 0.0, pressure = 0.0, dew_point = 0.0;
    float uvi = 0.0, wind_speed = 0.0, wind_gust = 0.0;
    int wind_deg = 0, visibility = 0, clouds = 0, current_icon = 800;
    String description = "--";
    String alert_text = "";
    String height_str = "--";
    float stable_height = 0.0;
    DailyForecast forecast[3]; 
};

struct EnvData {
    bool is_night_mode = false;
    bool is_alert_active = false;
    uint16_t icon_base_color = 0x2104; // Neutraal: Donkergrijs
    uint16_t alert_active_color = TFT_GREENYELLOW; // Alarm kleur
    // Deze helper geeft nu altijd de JUISTE kleur terug op basis van de status
    uint16_t get_current_alert_color() const {
        return is_alert_active ? alert_active_color : icon_base_color;
    }
    double sunrise_local = 0.0, sunset_local = 0.0;
    char sunrise_str[6] = "00:00", sunset_str[6] = "00:00";
    char current_time_str[10] = "00:00:00", current_date_str[12] = "01-01-2024";
    float temp_local = 0.0;
    float hum_local = 0.0;
    float press_local = 0.0;
    float altitude_local = 0.0;
    bool aht_online = false, bmp_online = false;
    double lat = 52.3702, lon = 4.8952; // Default: Amsterdam
    bool alert_muted = false; // Voor mute status van ISO-alerts
    bool is_touching = false; // Huidige status van aanraking (voor touch ladder)

};

struct NetworkState {
    String mdns = "";     // Nieuw: voor StorageManager
    String ssid = "";         // Nieuw: voor StorageManager
    String pass = "";     // Nieuw: voor StorageManager
    String owm_key = "";      // Nieuw: voor StorageManager
    bool is_updating = false;
    bool is_setup_mode = false;
    bool wifi_connected = false;
    int wifi_signal = 0;
    int wifi_mode = 1; // 0 = altijd aan, 1 = alleen bij updates, 2 = eco (uit in nachtmodus)
    bool pending_restart = false;
    unsigned long restart_at = 0;
    bool wifi_enabled = true; // De hoofdschakelaar
    bool is_op_active = false; // Is het Access Point actief? (voor setup)
    bool eco_mode_enabled = false; // Eco-modus (uit in nachtmodus)
    uint32_t last_sync_epoch = 0; // Wanneer hebben we voor het laatst succesvol gehad?
    bool web_server_active = false; // Staat de webserver aan?
    // unsigned long server_stop_at = 0; // Tijdstip waarop de server automatisch uitgaat (na setup)
    unsigned long server_start_time = 0;
};

struct DisplaySettings {
    // Gekoppeld aan Config uit config.h
    int backlight_pwm = Config::default_brightness;
    int day_brightness = Config::brightness_day;
    int night_brightness = Config::brightness_night;
    int transition_type = 0;
    int transition_speed = Config::default_transition_speed; 
    
    int ticker_x = 320;
    int total_ticker_width = 0;
    bool show_vandaag = true; 
    unsigned long last_panel_switch = 0;
    bool show_easter_egg = false;
    unsigned long next_egg_time = 0;
    bool is_animating = false;

    bool led_enabled = true;
    int led_brightness = 200;
};

// --- DE HOOFDSTRUCTUUR (S.P.O.T.) ---
struct SystemState {
    UserData user;
    WeatherData weather;
    EnvData env;
    DisplaySettings display;
    NetworkState network;
    std::vector<TickerSegment> ticker_segments;
};

// --- EXTERNS ---
extern SystemState state;
extern const char* dagNamen[];
extern const char* maandNamen[];
