/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Helper functies en display setup
 */

#pragma once
#include <TFT_eSPI.h>
#include <Adafruit_AHTX0.h>

// 1. Hardware instanties (extern blijven)
extern TFT_eSPI tft;
extern TFT_eSprite clkSpr, datSpr, datSpr2, tckSpr;
extern Adafruit_AHTX0 aht;

// 2. Hardware-gerelateerde functies
void setupDisplay();
void updateDisplayBrightness(int level);
void updateClock(); // Tekent de wijzers.

void updatePaneelvandaag(); // Tekent de data voor vandaag in het datSpr paneel
void updatePaneelmorgen(); // Tekent de data voor morgen in het datSpr2 panel
void updateTicker(); // Tekent de data in het ticker paneel

// 3. Teken-helpers (De "Vakmannen")
void drawSetupModeActive();
void toonNetwerkInfo();
void drawMoonPhase(int x, int y, int radius);
void drawWifiIndicator(int x, int y, int h);
void drawISOAlert(int x, int y, int size, uint16_t color, bool active);
void drawWeatherIcon(TFT_eSprite &spr, int x, int y, int size, int iconId, bool isDay);

uint16_t getIconColor(int iconId);
int getBeaufort(float ms);
String getWindRoos(int graden);

void handleHardware();          // voor touch en LED logica
void handleTouchToggle();       // Alleen de touch logica, zodat we die apart kunnen aanroepen indien nodig
void manageStatusLed();         // Voor het beheren van de LED-status op basis van de huidige staat
// void testTouchMetFeedback(); // Een testfunctie die je kunt gebruiken om de touch-gevoeligheid en feedback te testen zonder de volledige toggle-logica