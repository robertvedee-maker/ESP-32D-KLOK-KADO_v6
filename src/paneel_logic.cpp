/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Beheer van de data-panelen en de overgangen daartussen,
 * Het tekenen van de panelen wordt gedaan in helpers.cpp (om de code gescheiden te houden tussen 'wat' en 'hoe')
 */

#include "paneel_logic.h"
#include "clasic_clock.h"
#include "config.h"
#include "global_data.h"
#include "helpers.h"
#include "secret.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <SPI.h>

void renderTicker();
void updateClock();

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
