/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Weather data fetching and icon mapping
 */

#pragma once
#include <Arduino.h>

void manageWeatherUpdates();
bool fetchWeather(bool checkOnly = false);        // De hoofdfunctie die state.weather vult
void saveWeatherCache();    // Sla de laatste weerdata op in NVS voor offline gebruik
void loadWeatherCache();    // Laad de laatste weerdata uit NVS bij opstarten
String translateToNL(String englishText); // De hulp-functie voor alerts
bool forceFirstWeatherUpdate(); // Controleer of we bij opstarten een geforceerde update moeten uitvoeren

