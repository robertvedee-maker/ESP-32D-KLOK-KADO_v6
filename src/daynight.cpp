/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Day/Night and Brightness Management
 */

#include "daynight.h"
#include "global_data.h"
#include "secret.h"
#include <SolarCalculator.h>

// Forward declaration
void updateDisplayBrightness(int pwm);

void updateDateTimeStrings(struct tm* timeinfo)
{
    snprintf(state.env.current_time_str, sizeof(state.env.current_time_str),
        "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    snprintf(state.env.current_date_str, sizeof(state.env.current_date_str),
        "%02d-%02d-%04d", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
}

void manageTimeFunctions()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    time_t now = time(nullptr);
    double transit, sunrise, sunset;

    calcSunriseSunset(now, SECRET_LAT, SECRET_LON, transit, sunrise, sunset);

    double utcOffset = (timeinfo.tm_isdst > 0) ? 2.0 : 1.0;
    state.env.sunrise_local = sunrise + utcOffset;
    state.env.sunset_local = sunset + utcOffset;

    // Direct de strings vullen in de state
    int rH = (int)state.env.sunrise_local;
    int rM = (int)((state.env.sunrise_local - rH) * 60);
    snprintf(state.env.sunrise_str, sizeof(state.env.sunrise_str), "%02d:%02d", rH, rM);

    int sH = (int)state.env.sunset_local;
    int sM = (int)((state.env.sunset_local - sH) * 60);
    snprintf(state.env.sunset_str, sizeof(state.env.sunset_str), "%02d:%02d", sH, sM);
}

void manageBrightness()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    int nu = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
    int op = (int)(state.env.sunrise_local * 60);
    int onder = (int)(state.env.sunset_local * 60);

    int dag = state.display.day_brightness;
    int nacht = state.display.night_brightness;
    int target_pwm = state.display.backlight_pwm;

    // Gebruik solar_fade_minutes (60) in plaats van de 1000ms
   if (nu >= op && nu < (op + Config::solar_fade_minutes)) {
        target_pwm = map(nu, op, op + Config::solar_fade_minutes, nacht, dag);
        state.env.is_night_mode = false; // We zijn aan het opstaan (Dag-modus)
    } else if (nu >= onder && nu < (onder + Config::solar_fade_minutes)) {
        target_pwm = map(nu, onder, onder + Config::solar_fade_minutes, dag, nacht);
        state.env.is_night_mode = true;  // De zon gaat onder (Nacht-modus start)
    } else {
        if (nu >= (op + Config::solar_fade_minutes) && nu < onder) {
            target_pwm = dag;
            state.env.is_night_mode = false; // Volle dag
        } else {
            target_pwm = nacht;
            state.env.is_night_mode = true;  // HIER: Nu staat hij ook echt op nacht!
        }
    }

    // Serial.printf("[Brightness] Nu: %02d:%02d, Op: %02d:%02d, Onder: %02d:%02d, Target PWM: %d\n",
    //     timeinfo.tm_hour, timeinfo.tm_min,
    //     op / 60, op % 60,
    //     onder / 60, onder % 60,
    //     target_pwm);
    // Serial.printf(state.env.is_night_mode ? "[Brightness] Huidige modus: NACHT\n" : "[Brightness] Huidige modus: DAG\n");

    // Gebruik de berekende target_pwm om de hardware aan te sturen
    if (target_pwm != state.display.backlight_pwm) {
        // Alleen de functie aanroepen, de functie zelf werkt de state bij!
        updateDisplayBrightness(target_pwm);
    }
}
