/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Display logic functies
 */

#pragma once

// De grote drie voor de scherm-aansturing
// void updateTickerSegments(); // Vult state.ticker_segments
// void renderTicker();         // Verplaatst state.display.ticker_x en tekent
// void manageDataPanels();     // Wisselt tussen state.display.show_vandaag
// void updateDataPaneel2();    // Tekent de forecast uit state.weather.forecast

void renderFace(int h, int m, int s);
