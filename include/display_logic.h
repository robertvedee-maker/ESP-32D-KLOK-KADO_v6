/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Display logic functies
 */

#pragma once

#include <Arduino.h>

// De grote drie voor de scherm-aansturing
// void updateTickerSegments(); // Vult state.ticker_segments
// void renderTicker();         // Verplaatst state.display.ticker_x en tekent
// void manageDataPanels();     // Wisselt tussen state.display.show_vandaag
// void updateDataPaneel2();    // Tekent de forecast uit state.weather.forecast


// Forward declaration: We vertellen de compiler alleen DAT er een 
// klasse bestaat die TFT_eSprite heet, zonder de hele library te laden.
class TFT_eSprite; 

// De functie gebruikt nu alleen de referentie (pointers)
void performTransition(TFT_eSprite* oldSpr, TFT_eSprite* newSpr);

extern void updateDataPaneelVandaag();
extern void updateDataPaneelForecast();
extern void showSetupInstructionPanel();

void renderFace(int h, int m, int s);