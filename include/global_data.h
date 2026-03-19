/*
 * (c)2026 R van Dorland - Licensed under MIT License
 */

#pragma once

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>
#include "config.h" // We gebruiken Config:: voor de defaults

enum SystemHealth
{
    HEALTH_OK,
    HEALTH_WARNING,
    HEALTH_CRITICAL
};

enum TickerMode
{
    MODE_BOOT,   // Welkomstboodschap bij opstarten
    MODE_CONFIG, // Speciaal voor configuratie (zoals QR-code)
    MODE_NORMAL, // Reguliere weersinformatie
    MODE_ALERT,  // Geforceerd weeralarm (10 min of OWM alert)
    // MODE_WAITING // Optioneel: Specifieke status voor 'wachten op slot'
};

// --- TYPES & ENUMS ---
struct TickerSegment
{
    char text[128]; // Vaste buffer van 128 bytes, GEEN String!
    uint16_t color;
    int width;
};

struct UserData
{
    String name = "";
    String dob = ""; // YYYY-MM-DD
    String Leeftijd = "";
};

struct ActiveBirthday {
    String name;
    int d, m, j;
    int daysLeft;
    char gender; // 'M', 'V' of '?' als fallback
};

struct BirthdayEntry {
    String naam;
    int dag, maand, jaar;
    long dagenTeGaan;
    int nieuweLeeftijd;
    char gender; // 'M', 'V' of '?' als fallback
};


struct DailyForecast
{
    uint32_t dt = 0;
    float temp_min = 0.0;
    float temp_max = 0.0;
    int icon_id = 800;
    float moon_phase = 0.0;
    uint32_t moon_rise = 0;
    uint32_t moon_set = 0;
    char summary[128] = "";
    char description[32] = "";
};

// --- SUBSYSTEM STATES ---
struct WeatherData
{
    bool data_is_fresh = false; // Wordt pas true na de ALLEREERSTE geslaagde fetchWeather()

    uint32_t last_update_epoch = 0;
    float temp = 0.0, feels_like = 0.0, humidity = 0.0, pressure = 0.0, dew_point = 0.0;
    float uvi = 0.0, wind_speed = 0.0, wind_gust = 0.0;
    int wind_deg = 0, visibility = 0, clouds = 0, current_icon = 800;
    String description = "--";

    // Alarmen en waarschuwingen
    String alert_text = "", alert_event = "", alert_tags = "";
    long alert_start = 0, alert_end = 0;

    // SPECIFIEK VOOR VANDAAG (Dag 0 uit de daily-sectie)
    struct
    {
        char today_tomorrow_str[10] = "VANDAAG"; // Of "MORGEN", afhankelijk van het tijdstip
        char summary[128]; // De mooie zin van OWM3
        float temp_min;
        float temp_max;
        float temp_night;
        float temp_eve;
        float temp_morn;
        float moon_phase;
        uint32_t sun_rise;
        uint32_t sun_set;
        uint32_t moon_rise;
        uint32_t moon_set;
    } today;

    // Forecast voor de komende dagen
    DailyForecast forecast[4];

    String height_str = "--";
    float stable_height = 0.0;
};

struct EnvData
{
    // AHT20 BPM280 DS18B20 Status
    bool aht_ok = false;
    bool bmp_ok = false;
    bool ds18b20_ok = false;

    // Barometer
    float pressure = 1013.25; // Actuele druk van BMP280
    int baro_trend = 0;       // -1 = dalend, 0 = stabiel, 1 = stijgend
    float baro_ref_3h = 1013.25; // De referentie voor de trend, opgeslagen in NVS en bijgewerkt elke 3 uur

    // Gezondheidsscore en gerelateerde kleuren/statussen
    bool is_night_mode = false;
    bool is_alert_active = false;
    uint16_t ticker_color = TFT_LIGHTGREY;         // Neutraal: Lichtgrijs
    uint16_t icon_base_color = 0x2104;             // Neutraal: Donkergrijs
    uint16_t alert_active_color = TFT_GREENYELLOW; // Alarm kleur
    // Deze helper geeft nu altijd de JUISTE kleur terug op basis van de status
    uint16_t get_current_alert_color() const
    {
        if (health == HEALTH_CRITICAL)
            return 0xF800; // Rood
        if (health == HEALTH_WARNING)
            return 0xFDE0; // Geel/Oranje
        return is_alert_active ? alert_active_color : icon_base_color;
    }

    char alert_message[128] = "";

    double sunrise_local = 0.0, sunset_local = 0.0, moonrise_local = 0.0, moonset_local = 0.0;
    char sunrise_str[6] = "00:00", sunset_str[6] = "00:00";
    char current_time_str[10] = "00:00:00", current_date_str[14] = "ZO 01-01-2024";
    float raw_temp_local = 0.0; // De ruwe temperatuur van de AHT20, voor interne checks
    float temp_local = 0.0;
    float hum_local = 0.0;
    float press_local = 0.0;

    // float altitude_local = 0.0;
    bool aht_online = false, bmp_online = false;
    double lat = 52.3702, lon = 4.8952; // Default: Amsterdam
    bool alert_muted = false;           // Voor mute status van ISO-alerts
    bool is_touching = false;           // Huidige status van aanraking (voor touch ladder)

    // Interne CASE temperatuur (voor thermal safety checks)
    float case_temp = 0.0;
    SystemHealth health = HEALTH_OK;
    int next_easter_egg_minute = -1; // Voor het plannen van het volgende paasei
};

struct NetworkState
{
    String mdns = "";    // Nieuw: voor StorageManager
    String ssid = "";    // Nieuw: voor StorageManager
    String pass = "";    // Nieuw: voor StorageManager
    String owm_key = ""; // Nieuw: voor StorageManager
    bool is_updating = false;
    bool is_setup_mode = false;
    bool wifi_connected = false;
    int wifi_signal = 0;
    int wifi_mode = 1; // 0 = altijd aan, 1 = alleen bij updates, 2 = eco (uit in nachtmodus)
    bool pending_restart = false;
    unsigned long restart_at = 0;
    bool wifi_enabled = true;       // De hoofdschakelaar
    bool is_op_active = false;      // Is het Access Point actief? (voor setup)
    bool eco_mode_enabled = false;  // Eco-modus (uit in nachtmodus)
    uint32_t last_sync_epoch = 0;   // Wanneer hebben we voor het laatst succesvol gehad?
    bool web_server_active = false; // Staat de webserver aan?
    // unsigned long server_stop_at = 0; // Tijdstip waarop de server automatisch uitgaat (na setup)
    unsigned long server_start_time = 0;
};

struct DisplaySettings
{
    // Gekoppeld aan Config uit config.h
    int backlight_pwm = Config::default_brightness;
    int day_brightness = Config::brightness_day;
    int night_brightness = Config::brightness_night;
    int transition_type = 0;
    int transition_speed = Config::default_transition_speed;

    TickerMode current_ticker_mode = MODE_BOOT; // De huidige actieve modus
    bool force_ticker_refresh = false;          // Vlag om direct van rechts te starten

    // Verjaardagskalender
    bool show_calendar = false;
    unsigned long calendar_show_until = 2 * 60 * 1000; // Tot wanneer de kalender getoond moet worden (2 minuten na activeren)
    bool birthday_upcoming = false; // Huidige status of er een verjaardag binnen 5 dagen is, voor snelle checks in de klok
    char birthday_gender = '?'; // Huidige gender van de verjaardag (? = onbekend)

    int ticker_x = 320;
    int total_ticker_width = 0;
    bool show_vandaag = true;
    unsigned long last_panel_switch = 0;
    bool show_easter_egg = false;
    unsigned long next_egg_time = 0;
    bool is_animating = false;

    uint16_t touch_indicator_color = TFT_BLACK; // Kleur van de touch-indicator (kan dynamisch veranderen)

    unsigned long alert_show_until = 0; // Wanneer moeten we de alert niet meer tonen (na einde + buffer)
    bool force_alert_display = false;   // Forceren we de alert weergave

    bool show_config_qr = false; // Is de QR-code nu zichtbaar in datSpr?

    bool show_system_monitor = false;
    bool show_sm_bg_drawn = false;

    bool led_enabled = true;
    int led_brightness = 200;
};

// --- DE HOOFDSTRUCTUUR (S.P.O.T.) ---
struct SystemState
{
    UserData user;
    std::vector<ActiveBirthday> currentBirthdays;
    WeatherData weather;
    EnvData env;
    DisplaySettings display;
    NetworkState network;
    std::vector<TickerSegment> ticker_segments;
};

// --- EXTERNS ---
extern SystemState state;
extern const char *dagNamen[];
extern const char *maandNamen[];
extern std::vector<TickerSegment> tickerSegments;
extern std::vector<ActiveBirthday> currentBirthdays;

// De prefix 'RTC_DATA_ATTR' zorgt dat dit in het speciale geheugen komt
extern RTC_DATA_ATTR uint32_t lastOWMFetchTime0;
extern RTC_DATA_ATTR uint32_t lastSensorFetchTime0;
