/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Day/Night and Brightness Management
 */

#include "daynight.h"
#include "global_data.h"
#include "secret.h"
#include <SolarCalculator.h>

// Forward declaration
void manageBrightness();
void manageTimeFunctions();
void updateDisplayBrightness(int pwm);

//      --- HELDERHEID EN ZON-LOGICA: Beheer van de automatische aanpassing van de helderheid op basis van tijd en veiligheid ---
void updateDateTimeStrings() // Geen parameters nodig, we halen het zelf op
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Dagen van de week (0 = Zondag)
    const char* days[] = {"ZO", "MA", "DI", "WO", "DO", "VR", "ZA"};

    // Tijd: HH:MM:SS
    snprintf(state.env.current_time_str, sizeof(state.env.current_time_str),
             "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Datum: DAG DD-MM-YYYY
    snprintf(state.env.current_date_str, sizeof(state.env.current_date_str),
             "%s %02d-%02d-%04d", days[timeinfo.tm_wday], timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}

// void updateDateTimeStrings(struct tm *timeinfo)
// {
//     snprintf(state.env.current_time_str, sizeof(state.env.current_time_str),
//              "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

//     snprintf(state.env.current_date_str, sizeof(state.env.current_date_str),
//              "%02d-%02d-%04d", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
// }

//      --- HOOFDLOGICA VOOR HET BEHEREN VAN DE TIJD EN HELDERHEID (Zon-Logica + Veiligheids-override) ---
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

//      --- HOOFDFUNCTIE VOOR HET BEHEREN VAN DE HELDERHEID OP BASIS VAN ZON-LOGICA EN VEILIGHEIDS-OVERRIDE ---
void manageBrightness()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    // 1. DATA VOORBEREIDEN
    int nu = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
    int op = (int)(state.env.sunrise_local * 60);
    int onder = (int)(state.env.sunset_local * 60);
    int fade = Config::solar_fade_minutes;

    int dag = state.display.day_brightness;
    int nacht = state.display.night_brightness;
    int target_pwm = nacht; // We gaan uit van nacht als default

    // 2. BASIS HELDERHEID BEPALEN (De Zon-logica)
    if (nu >= (op - fade) && nu < op) {
        // Zonsopkomst (Fade IN)
        target_pwm = map(nu, op - fade, op, nacht, dag);
        state.env.is_night_mode = false;
    } 
    else if (nu >= op && nu < onder) {
        // Volle dag
        target_pwm = dag;
        state.env.is_night_mode = false;
    } 
    else if (nu >= onder && nu < (onder + fade)) {
        // Zonsondergang (Fade OUT)
        target_pwm = map(nu, onder, onder + fade, dag, nacht);
        state.env.is_night_mode = false; 
    } 
    else {
        // Volle nacht
        target_pwm = nacht;
        state.env.is_night_mode = true;
    }

    // 3. THERMISCHE INJECTIE (De Veiligheids-override)
    // Deze komt ná de zon-logica zodat veiligheid altijd wint
    if (state.env.health == HEALTH_CRITICAL) {
        target_pwm = min(target_pwm, 15);
    } 
    else if (state.env.health == HEALTH_WARNING) {
        target_pwm = target_pwm / 2;
    }

    // 4. HARDWARE AANSTUREN
    // We checken hier slechts één keer of de waarde echt veranderd is
    if (target_pwm != state.display.backlight_pwm) {
        updateDisplayBrightness(target_pwm);
        Serial.printf("[Brightness] Nieuwe PWM: %d \n", target_pwm);
        Serial.printf("[HEALTH] %d, Temperatuur Behuizing: %.1f°C \n", state.env.health, state.env.case_temp);
    }
}

