#include "global_data.h"
#include "helpers.h"
#include "leeftijd_calc.h" // Voor het eitje
#include "secret.h"        // Voor de namen/bday
#include "weather_logic.h" // Voor de forecast data

// Forward declaration
String getGepersonaliseerdFeitje();

// Helper om segmenten toe te voegen aan de state
void addSegment(String t, uint16_t c)
{
    int w = tckSpr.textWidth(t);
    state.ticker_segments.push_back({t, c, w});
    state.display.total_ticker_width += w;
}

void updateTickerSegments()
{
    state.ticker_segments.clear();
    state.display.total_ticker_width = 0;
    uint16_t baseColor = TFT_LIGHTGREY;

    // 1. Alerts
    if (state.env.is_alert_active && !state.env.alert_muted && state.weather.alert_text.length() > 0)
    {
        String alert = " !!! " + state.weather.alert_text + " !!! ";
        alert.toUpperCase();
        addSegment(alert, state.env.get_current_alert_color());
        return; // Bij een actieve alert tonen we alleen de alert, geen andere info
    }

    if (state.display.show_easter_egg)
    {
        String eggStr = getGepersonaliseerdFeitje();

        // Als eggStr leeg is (door de check hierboven), wordt er niets toegevoegd
        if (eggStr != "")
        {
            addSegment(eggStr, TFT_YELLOW);
        }
    }

    // 3. Hoogte
    if (state.weather.height_str != "0")
    {
        addSegment(" | HOOGTE: " + state.weather.height_str + " m b.NAP", baseColor);
    }

    addSegment(" | GEVOELSTEMPERATUUR: " + String(state.weather.feels_like, 1) + "c", baseColor);

    // 4. Weer Details (Dauwpunt & Bewolking)
    addSegment(" | DAUWPUNT: " + String(state.weather.dew_point, 1) + "c | BEWOLKING: " + String(state.weather.clouds) + "%",
               baseColor);

    // 5. Zicht (Met de floating point 'eitje' logica)
    String zichtStr = (state.weather.visibility >= 1000)
                          ? String(state.weather.visibility / 1000.0, 1) + " km"
                          : String(state.weather.visibility) + " m";
    addSegment(" | ZICHT: " + zichtStr, baseColor);

    // 6. Wind (Met de "lelijke" / 0.27778 berekening)
    float speedMs = state.weather.wind_speed / 0.27778;
    addSegment(" | WIND: " + String(speedMs, 1) + " m/s (" + getWindRoos(state.weather.wind_deg) + ")",
               baseColor);

    if (state.weather.wind_gust / 0.27778 > speedMs + 1.5)
    {
        addSegment(" | WINDVLAGEN: " + String(state.weather.wind_gust / 0.27778, 1) + " m/s", baseColor);
    }

    addSegment("  +++  ", baseColor);
}

void renderTicker()
{
    tckSpr.fillSprite(TFT_BLACK);
    tckSpr.setTextFont(2);
    tckSpr.setTextDatum(TL_DATUM);

    // 1. Teken Set A en Set B
    int currentX = state.display.ticker_x;
    for (int i = 0; i < 2; i++)
    { // Lus voor beide sets
        int startX = currentX + (i * state.display.total_ticker_width);
        for (auto &seg : state.ticker_segments)
        {
            tckSpr.setTextColor(seg.color, TFT_BLACK);
            tckSpr.drawString(seg.text, startX, 0);
            startX += seg.width;
        }
    }

    // Push naar de S.P.O.T. locatie (onderkant scherm)
    tckSpr.pushSprite(0, Config::get_ticker_y(tft.height()));
    state.display.ticker_x -= 1;

    // --- DE VEILIGE WISSEL LOGICA ---
    // Check 1: Halverwege de animatie (wanneer Set A uit beeld is) berekenen we de NIEUWE data vast voor
    static bool updatePending = false;
    if (state.display.ticker_x < -(state.display.total_ticker_width / 2) && !updatePending)
    {
        // We berekenen de nieuwe segmenten nog NIET, maar we zetten de vlag
        // Of we berekenen ze in een 'schaduw' variabele als je dat wilt.
        updatePending = true;
    }

    // Check 2: De RESET (Hét moment voor de wissel)
    if (state.display.ticker_x <= -state.display.total_ticker_width)
    {
        // Voer de zware update uit op het moment dat x weer naar 0 springt
        updateTickerSegments();

        state.display.ticker_x = 0;
        updatePending = false;
        // Serial.println(F("[TICKER] Naadloze wissel voltooid"));
    }
}
