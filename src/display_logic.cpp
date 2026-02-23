/*
 * (c)2026 R van Dorland - Licensed under MIT License
*/

#include "display_logic.h"
#include "clasic_clock.h"
#include "config.h"

#include "global_data.h"
#include "helpers.h"
#include "leeftijd_calc.h" // Voor het eitje
#include "secret.h"        // Voor de namen/bday
#include "weather_logic.h" // Voor de forecast data

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <SPI.h>

// 1. Hardware instanties (MOETEN hier gedefinieerd worden)
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite clkSpr = TFT_eSprite(&tft);
TFT_eSprite datSpr = TFT_eSprite(&tft);
TFT_eSprite datSpr2 = TFT_eSprite(&tft);
TFT_eSprite tckSpr = TFT_eSprite(&tft);

extern SystemState state; // Voor toegang tot de centrale 'state' struct

void renderTicker();
void updateClock();



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


//          --- PANEEL LOGIC: Beheer van de data-panelen en de overgangen daartussen ---
void performTransition(TFT_eSprite* oldSpr, TFT_eSprite* newSpr)
{

    if (oldSpr == nullptr || newSpr == nullptr)
        return;
    if (oldSpr->width() == 0 || newSpr->width() == 0) {
        newSpr->pushSprite(Config::data_x, Config::data_y);
        return;
    }

    int w = oldSpr->width();
    int h = oldSpr->height();
    int x = Config::data_x;
    int y = Config::data_y;
    int speed = state.display.transition_speed;
    // VEILIGHEID: Als speed 0 of lager is, forceer een geldige waarde
    if (speed <= 0)
        speed = 8;

    // --- DE MAGIE: Zet het 'hekje' om het data-vlak ---
    // Alles wat buiten deze coördinaten getekend wordt, wordt genegeerd.
    tft.setViewport(x, y, w, h);

    for (int i = 0; i <= ((state.display.transition_type >= 2) ? h : w); i += speed) {
        // Binnen de viewport is (x, y) nu (0, 0)
        switch (state.display.transition_type) {
        case 0: // SLIDE_LEFT
            oldSpr->pushSprite(-i, 0);
            newSpr->pushSprite(w - i, 0);
            break;
        case 1: // SLIDE_RIGHT
            oldSpr->pushSprite(i, 0);
            newSpr->pushSprite(-w + i, 0);
            break;
        case 2: // SLIDE_UP
            oldSpr->pushSprite(0, -i);
            newSpr->pushSprite(0, h - i);
            break;
        case 3: // SLIDE_DOWN
            oldSpr->pushSprite(0, i);
            newSpr->pushSprite(0, -h + i);
            break;
        default:
            newSpr->pushSprite(0, 0);
            i = 9999; // Stop loop
            break;
        }
        tft.resetViewport();

        // We verplaatsen de ticker-positie handmatig met 1 pixel 
        // (of hetzelfde tempo als de 'speed' van de wissel)
        state.display.ticker_x -= 1;

        renderTicker();
        updateClock();
        tft.setViewport(x, y, w, h); // Terug naar het 'hekje' voor de volgende frame

        yield();
    }

    // --- RESET: Haal het hekje weer weg voor de rest van het scherm ---
    tft.resetViewport();
    newSpr->pushSprite(x, y);
}

//          --- HOOFDFUNCTIE: Beheer van de data-panelen en de wissel daartussen ---
void manageDataPanels()
{

    // // 1. Check of OWM geconfigureerd is
    // if (state.network.owm_key.length() < 10) {
    //     // Stop de automatische wissel en toon het 'Setup' paneel
    //     showSetupInstructionPanel();
    //     return; 
    // }

    unsigned long nu = millis();
    unsigned long interval = state.display.show_vandaag ? Config::time_display_long : Config::time_display_short;

    // Serial.printf("Panel: %d, Delta: %lu, Target: %lu\n", state.display.show_vandaag, millis() - state.display.last_panel_switch, interval);

    // 1. Is het tijd voor de wissel?
    if (nu - state.display.last_panel_switch > interval) {
        // Stoplicht check: Geen animatie tijdens WiFi-updates
        if (state.network.is_updating)
            return;

        if (state.display.show_vandaag) {
            // Serial.println(">>> Start wissel naar Forecast");
            updateDataPaneelForecast();

            // STAP B: Korte pauze (10-20ms) zodat de ESP32 de sprite-buffer kan afsluiten
            delay(20);

            performTransition(&datSpr, &datSpr2);
            state.display.show_vandaag = false;
        } else {
            // Serial.println(">>> Start wissel naar Vandaag");
            updateDataPaneelVandaag();

            // STAP B: Korte pauze (10-20ms) zodat de ESP32 de sprite-buffer kan afsluiten
            delay(20);

            performTransition(&datSpr2, &datSpr);
            state.display.show_vandaag = true;
        }

        state.display.last_panel_switch = nu;
        return; // Verlaat functie, we zijn klaar voor deze frame
    }

    // 2. Statische Update (Alleen als er geen netwerk-activiteit is)
    if (!state.network.is_updating) {
        if (state.display.show_vandaag) {
            updateDataPaneelVandaag();
            datSpr.pushSprite(Config::data_x, Config::data_y);
        } else {
            updateDataPaneelForecast();
            datSpr2.pushSprite(Config::data_x, Config::data_y);
        }
    }
}
