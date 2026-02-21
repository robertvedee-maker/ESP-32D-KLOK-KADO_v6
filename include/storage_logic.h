#pragma once

// Dit is de 'inhoudsopgave' voor de compiler
void initStorage();

void loadUserData();       // Laadt naam en geboortedatum

void saveDisplaySettings();
void loadDisplaySettings(); // Laadt Helderheid, Transitie, User en LED

void saveNetworkConfig();
void loadNetworkConfig();   // Laadt SSID/Pass

void saveOMWConfig();
void loadOMWConfig();

void loadWeatherCache();    // Laadt de laatste weerdata
