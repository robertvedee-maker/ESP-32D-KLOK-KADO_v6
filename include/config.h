/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * General configuration settings
 * 
 *  --- !!! 
 * Waar in het script SECRET_xxx staat komt de informatie uit secret.h, 
 * zodat die makkelijk te vinden is en niet per ongeluk in de code terechtkomt. 
 * Zorg ervoor dat je secret.h niet deelt of publiceert, want daar staan je WiFi-gegevens en API keys in! 
 *  --- !!!
 */

#pragma once

#include <TFT_eSPI.h>
#include <cstdint>

namespace Config
{
    // --- 1. HARDWARE PINNEN ---
    constexpr int pin_backlight = TFT_BL;           // GPIO pin voor TFT backlight (meestal TFT_BL, maar controleer je scherm)
    constexpr int pin_fingerprint_led = 2;          // GPIO2 is de ingebouwde LED op veel ESP32 boards, en zit vaak achter de vingerprint sensor
    constexpr int pin_OneWire = 32;                 // GPIO32 voor DS18B20 (OneWire bus)
    constexpr int pin_buck_enable = PIN_BUCK_EN;    // GPIO27 om de buck converter aan/uit te zetten (voor stroombeheer)

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

    inline constexpr int time_display_long = 45000;  // ms dat de tijd getoond wordt bij wissel
    inline constexpr int time_display_short = 15000; // ms dat de tijd getoond wordt bij wissel

    // --- 4. HARDWARE INSTELLINGEN (PWM & Helderheid) ---
    constexpr int pwm_freq = 20000;
    constexpr int pwm_res = 8;

    constexpr int default_brightness = 10;          // Start helderheid bij opstarten (0-255)
    constexpr int brightness_day = 180;             // Helderheid overdag (0-255)
    constexpr int brightness_night = 20;            // Helderheid 's nachts (0-255)
    constexpr int fade_duration = 1000;             // ms
    constexpr int solar_fade_minutes = 30;          // min
    constexpr int default_transition_speed = 12;    // ms
    constexpr int default_transition_type = 1;      // 0 = slide left, 1 = slide right, 2 = slide up, 3 = slide down, 4 = none
    constexpr int distance_per_hour = 14;             // pixels per uur voor de zon- en maanbaan in het data-paneel (gebaseerd op 15 pixels per uur bij een 10 uur horizon)
    constexpr long ten_min_timeout = 10 * 60 * 1000UL; // ms dat de setup-modus actief blijft zonder verbinding voordat hij automatisch afsluit

    // --- 5. OVERIGE INSTELLINGEN ---
    constexpr uint16_t bg_color_day = TFT_DARKGREY;
    constexpr uint16_t bg_color_night = TFT_BLUE;
    constexpr uint16_t text_color_day = TFT_WHITE;
    constexpr uint16_t text_color_night = TFT_SKYBLUE;
    constexpr uint16_t gfx_color_day = TFT_LIGHTGREY;
    constexpr uint16_t gfx_color_night = TFT_NAVY /*TFT_SKYBLUE*/;

    constexpr int alert_timeout = 2 * 60 * 1000; // ms dat een Alert actief blijft in de ticker voordat hij automatisch terugkeert naar de normale status

    constexpr float system_temp_threshold = 72;  // °C waarbij we de buck converter uitschakelen om schade te voorkomen
    constexpr float system_temp_warning = 60;    // °C waarbij we een waarschuwing geven (dmv het ISOAlarm icoon en de backlight dimmen)
    constexpr float system_temp_hysteresis = 55; // °C waarbij we de buck converter weer inschakelen nadat deze is uitgeschakeld vanwege oververhitting

    // OpenWeatherMap 3.0 OneCall API
    // Gebruik: Config::owm_url + state.env.lat + "&lon=" + state.env.lon + "&appid=" + SECRET_OWM_KEY
    constexpr const char *owm_base_url = "http://api.openweathermap.org/data/3.0/onecall?";
    extern bool forceFirstWeatherUpdate; // Zet op 'true' om bij elke herstart direct een weerupdate te forceren (handig voor testen)

}
