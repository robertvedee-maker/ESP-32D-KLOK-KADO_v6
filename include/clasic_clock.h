/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Clasic Clock rendering logic
 */

#pragma once // Include this file only once

#include "config.h"
#include <TFT_eSPI.h>

extern TFT_eSPI tft; // TFT instance from helpers.h
extern TFT_eSprite clkSpr; // The clock sprite

void setupClassicClock();
// void renderFace(int h, int m, int s);
void getCoord(int16_t x, int16_t y, float* xp, float* yp, int16_t r, float a);
