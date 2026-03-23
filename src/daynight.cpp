/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Day/Night and Brightness Management
 */

#include "app_actions.h"


static float lastSunrise = -1.0; // Voor debuggen: onthoud de laatste sunrise tijd om te zien wanneer deze verandert

//      --- HELDERHEID EN ZON-LOGICA: Beheer van de automatische aanpassing van de helderheid op basis van tijd en veiligheid ---
void App::updateDateTimeStrings() // Geen parameters nodig, we halen het zelf op
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Dagen van de week (0 = Zondag)
    const char *days[] = {"ZO", "MA", "DI", "WO", "DO", "VR", "ZA"};

    // Tijd: HH:MM:SS
    snprintf(state.env.current_time_str, sizeof(state.env.current_time_str),
             "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Datum: DAG DD-MM-YYYY
    snprintf(state.env.current_date_str, sizeof(state.env.current_date_str),
             "%s %02d-%02d-%04d", days[timeinfo.tm_wday], timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}

//      --- HOOFDLOGICA VOOR HET BEHEREN VAN DE TIJD EN HELDERHEID (Zon-Logica + Veiligheids-override) ---
void App::manageTimeFunctions()
{

    // Helper function to convert epoch time to decimal hours
    auto getDecimalTime = [](time_t epoch) -> float
    {
        if (epoch == 0)
            return -100.0f; // Buiten beeld als er geen data is
        struct tm ltm;
        localtime_r(&epoch, &ltm); // Gebruik de veilige variant
        return ltm.tm_hour + (ltm.tm_min / 60.0f);
    };

    // --- 1. TIJD SYNC CHECK ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    // --- 2. ZON: EPOCH -> DECIMAL & STRINGS ---
    if (state.weather.today.sun_rise > 0)
    {
        state.env.sunrise_local = getDecimalTime((time_t)state.weather.today.sun_rise);
        state.env.sunrise_next_local = getDecimalTime((time_t)state.weather.today.sun_rise_tomorrow);
        state.env.sunset_local = getDecimalTime((time_t)state.weather.today.sun_set);

        // Strings vullen voor de labels (Sunrise)
        int rH = (int)state.env.sunrise_local;
        int rM = (int)((state.env.sunrise_local - rH) * 60);
        snprintf(state.env.sunrise_str, sizeof(state.env.sunrise_str), "%02d:%02d", rH, rM);
        int sHn = (int)state.env.sunrise_next_local;
        int sMn = (int)((state.env.sunrise_next_local - sHn) * 60);
        snprintf(state.env.sunrise_next_str, sizeof(state.env.sunrise_next_str), "%02d:%02d", sHn, sMn);
        // Strings vullen voor de labels (Sunset)
        int sH = (int)state.env.sunset_local;
        int sM = (int)((state.env.sunset_local - sH) * 60);
        snprintf(state.env.sunset_str, sizeof(state.env.sunset_str), "%02d:%02d", sH, sM);
    }

    // --- 3. MAAN: EPOCH -> DECIMAL ---
    // We hergebruiken getDecimalTime zodat de maan op dezelfde schaal als de zon staat
    if (state.weather.today.moon_rise > 0)
    {
        // Deze variabelen kun je nu direct gebruiken in je distMaanOp berekening
        state.env.moonrise_local = getDecimalTime((time_t)state.weather.today.moon_rise);
        state.env.moonset_local = getDecimalTime((time_t)state.weather.today.moon_set);
        state.env.moonset_next_local = getDecimalTime((time_t)state.weather.today.moon_set_tomorrow);
    }

    if (state.env.sunrise_local != lastSunrise)
    {
        lastSunrise = state.env.sunrise_local;
        // Serial.printf("[TIME] Sunrise: %.2f, Sunset: %.2f, Moonrise: %.2f, Moonset: %.2f, Sunrise Next: %.2f, Moonset Next: %.2f\n",
        //               state.env.sunrise_local, state.env.sunset_local,
        //               state.env.moonrise_local, state.env.moonset_local,
        //               state.env.sunrise_next_local, state.env.moonset_next_local);
    }
}

//      --- HOOFDFUNCTIE VOOR HET BEHEREN VAN DE HELDERHEID OP BASIS VAN ZON-LOGICA EN VEILIGHEIDS-OVERRIDE ---
void App::manageBrightness()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    // 1. DATA VOORBEREIDEN
    int nu = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
    int op = (int)(state.env.sunrise_local * 60);
    int onder = (int)(state.env.sunset_local * 60);
    int fade = Config::solar_fade_minutes;

    int dag = state.display.day_brightness;
    int nacht = state.display.night_brightness;
    int target_pwm = Config::default_brightness;

    // 2. BASIS HELDERHEID BEPALEN (De Zon-logica)
    if (nu >= (op - fade) && nu < op)
    {
        // Zonsopkomst (Fade IN)
        target_pwm = map(nu, op - fade, op, nacht, dag);
        state.env.is_night_mode = false;
    }
    else if (nu >= op && nu < onder)
    {
        // Volle dag
        target_pwm = dag;
        state.env.is_night_mode = false;
    }
    else if (nu >= onder && nu < (onder + fade))
    {
        // Zonsondergang (Fade OUT)
        target_pwm = map(nu, onder, onder + fade, dag, nacht);
        state.env.is_night_mode = false;
    }
    else
    {
        // Volle nacht
        target_pwm = nacht;
        state.env.is_night_mode = true;
    }

    // 3. THERMISCHE INJECTIE (De Veiligheids-override)
    // Deze komt ná de zon-logica zodat veiligheid altijd wint
    if (state.env.health == HEALTH_CRITICAL)
    {
        target_pwm = min(target_pwm, 15);
    }
    else if (state.env.health == HEALTH_WARNING)
    {
        target_pwm = Config::default_brightness;
    }

    // 4. HARDWARE AANSTUREN
    // We checken hier slechts één keer of de waarde echt veranderd is
    if (target_pwm != state.display.backlight_pwm)
    {
        updateDisplayBrightness(target_pwm);
        // Serial.printf("[Brightness] Nieuwe PWM: %d \n", target_pwm);
        // Serial.printf("[HEALTH] %d, Temperatuur Behuizing: %.1f°C \n", state.env.health, state.env.case_temp);
    }
}
