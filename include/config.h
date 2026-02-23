/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * General configuration settings
 */

#pragma once

#include <TFT_eSPI.h>
#include <cstdint>

namespace Config {
    
    // --- 1. HARDWARE PINNEN ---
    constexpr int pin_backlight = TFT_BL;
    constexpr int pin_OneWire = 32; // GPIO32 voor DS18B20 (OneWire bus)

    // --- 2. LAYOUT S.P.O.T. (Afmetingen) ---
    // Hier stel je de basis in. Verander je dit? Dan verandert de rest mee.
    inline constexpr int ticker_h = 20; // Hoogte van de ticker
    inline constexpr int clock_w = 150; // Breedte van de klok (vierkant 150x150)
    inline constexpr int clock_h = 150; // Hoogte van de klok

    // De data-panelen vullen de rest van de breedte op
    // Op een 320 breed scherm wordt dit 320 - 150 = 170.
    inline int get_data_width(int screen_w) { return screen_w - clock_w; }
    // De hoogte van de data-panelen is gelijk aan de klok voor een strakke lijn
    inline constexpr int data_h = clock_h;

    // --- 3. DYNAMISCHE POSITIES (Coördinaten) ---
    // Klok staat linksboven
    inline constexpr int clock_x = 0;
    inline constexpr int clock_y = 0;

    // Data panelen staan rechts van de klok
    inline constexpr int data_x = clock_w;
    inline constexpr int data_y = 0;

    // Ticker staat altijd onderaan, over de volle breedte
    inline int get_ticker_y(int screen_h) { return screen_h - ticker_h; }
    inline constexpr int ticker_x = 0;

    inline constexpr int time_display_long = 45000; // ms dat de tijd getoond wordt bij wissel
    inline constexpr int time_display_short = 15000; // ms dat de tijd getoond wordt bij wissel

    // --- 4. HARDWARE INSTELLINGEN (PWM & Helderheid) ---
    constexpr int pwm_freq = 20000;
    constexpr int pwm_res = 8;

    constexpr int default_brightness = 10;
    constexpr int brightness_day = 180;
    constexpr int brightness_night = 20;
    constexpr int fade_duration = 1000; // ms
    constexpr int solar_fade_minutes = 60; // min
    constexpr int default_transition_speed = 15; // ms
    constexpr int default_transition_type = 1 ; // 0 = slide left, 1 = slide right, 2 = slide up, 3 = slide down, 4 = none

    // --- 5. OVERIGE INSTELLINGEN ---
    constexpr uint16_t bg_color_day = TFT_DARKGREY;
    constexpr uint16_t bg_color_night = TFT_BLUE;
    constexpr uint16_t text_color_day = TFT_WHITE;
    constexpr uint16_t text_color_night = TFT_SKYBLUE;
    constexpr uint16_t gfx_color_day = TFT_LIGHTGREY;
    constexpr uint16_t gfx_color_night = TFT_NAVY/*TFT_SKYBLUE*/;

    constexpr int alert_timeout = 2 * 60 * 1000; // ms dat een ISO-alert actief blijft

    // OpenWeatherMap 3.0 OneCall API
    // Gebruik: Config::owm_url + state.env.lat + "&lon=" + state.env.lon + "&appid=" + SECRET_OWM_KEY
    constexpr const char* owm_base_url = "http://api.openweathermap.org/data/3.0/onecall?";
    extern bool forceFirstWeatherUpdate; // Zet op 'true' om bij elke herstart direct een weerupdate te forceren (handig voor testen)
    

}
