 /*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Day/Night and Brightness Management
 */

#pragma once
#include <Arduino.h>

// Alleen de actie-functies blijven over
void manageTimeFunctions(); // Berekent zon-tijden en vult state.env
void manageBrightness();    // Regelt de fade en vult state.display.backlight_pwm
void updateDateTimeStrings(struct tm *timeinfo); // Vult de char-arrays in state.env

