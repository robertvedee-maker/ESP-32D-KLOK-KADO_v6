/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * We gebruiken deze struct om een centrale 'state' te creëren die overal toegankelijk is.
 * In deze file staan ook de functies die in de loop() en setup() worden aangeroepen, 
 * zodat we een centrale plek hebben voor alle 'acties' die de app kan uitvoeren.
 * 
 * De functies zelf worden gedefinieerd in de respectievelijke .cpp files, 
 * maar hier beloven we dat ze bestaan en kunnen we ze aanroepen vanuit main.cpp zonder dat die file vol staat met alle details van de implementatie.
 * Op deze manier houden we de main loop overzichtelijk en kunnen we de implementatie van elke functie netjes gescheiden houden in aparte files, 
 * terwijl we toch een centrale plek hebben waar alle 'acties' worden gedefinieerd en beheerd.
 */

#pragma once

// 1. De centrale 'state' struct die de huidige status van de app bijhoudt (zoals sensorwaarden, netwerkstatus, display-instellingen, etc.) 
#include "clasic_clock.h"       // Voor de klokdata 
#include "config.h"             // Voor pinnen en constanten
#include "daynight.h"           // Voor tijd en helderheid
#include "display_logic.h"      // Voor ticker en panelen
#include "env_sensors.h"        // Voor BME/AHT sensoren
#include "global_data.h"        // Voor toegang tot 'state'
#include "helpers.h"            // Voor setupDisplay, updateClock, etc.
#include "leeftijd_calc.h"      // Voor het berekenen van leeftijd en ongewone weetjes op basis van geboortedatum
#include "network_logic.h"      // Voor WiFi en OTA setup
#include "paneel_logic.h"       // Voor het beheren van de data-panelen en de overgangen daartussen
#include "secret.h"             // Voor WiFi credentials
#include "storage_logic.h"      // Voor NVS opslag
#include "weather_logic.h"      // Voor OWM data
#include "web_config.h"         // Voor de webserver en captive portal tijdens setup mode

#include "bitmaps/weatherIcons40.h" // Voor de kleine weer-icoontjes in het 'vandaag' paneel
#include "bitmaps/weatherIcons68.h" // Voor de grotere weer-icoontjes in het 'forecast' paneel
#include "bitmaps/BootImages.h"     // Voor de opstart- en foutafbeeldingen

// 2. De functies die in de loop() en setup() worden aangeroepen, zodat we een centrale plek hebben voor alle 'acties' die de app kan uitvoeren.
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <time.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <cstdint>
#include <vector>

namespace App
{
    // clasic_clock.h
    void setupClassicClock(); // Voorbereidingen voor de klassieke klok (zoals het maken van sprites)

    // daynight.h
    void manageTimeFunctions();   // Berekent zon-tijden en vult state.env
    void manageBrightness();      // Regelt de fade en vult state.display.backlight_pwm
    void updateDateTimeStrings(); // Vult de char-arrays in state.env

    // display_logic.h
    void updateTickerSegments(); // Vult state.ticker_segments

    // env_sensors.h
    bool setupSensors();  // Initialiseer de sensoren (BME/AHT)
    void handleSensors(); // Lees de sensoren en update state.env

    // helpers.h
    void setupDisplay();            // Initialiseer het display en de sprites
    void updateDisplayBrightness(); // Pas de helderheid aan met een hardware-fade
    void drawSetupModeActive();
    void toonNetwerkInfo();        // Toont netwerk-info zoals IP-adres op het scherm
    void updateClock();             // Tekent de wijzers.
    void drawMoonPhase();           // Tekent de maan-fase op de gegeven positie
    void drawWifiIndicator();       // Tekent de WiFi-indicator op de gegeven positie
    void drawISOAlert();            // Tekent een ISO-alert (bijvoorbeeld voor regen) op de gegeven positie
    void drawWeatherIcon();         // Tekent een weer-icoon op de gegeven positie binnen de sprite
    uint16_t getIconColor();        // Bepaalt de kleur van een weer-icoon op basis van het iconId
    int getBeaufort();              // Converteert windsnelheid in m/s naar de Beaufort-schaal
    String getWindRoos();           // Converteert windrichting in graden naar een windroos (N, NE, E, etc.)
    void handleHardware();          // Voor touch en LED logica
    void handleTouchToggle();       // Alleen de touch logica, zodat we die apart kunnen aanroepen indien nodig
    void manageStatusLed();         // Voor het beheren van de LED-status op basis van de touch input

    // leeftijd_calc.h
    struct Leeftijd;
    void toonPersoonlijkeInfo(); // Toont persoonlijke info zoals leeftijd op basis van geboortedatum

    // network_logic.h
    void setupWiFi();              // Voorbereidingen voor WiFi (zoals het starten van de setup mode)
    void startSetupMode();         // Start de WiFi setup mode (AP + captive portal
    void setupOTA();              // Voorbereidingen voor OTA updates
    void disableWiFi();           // Schakel WiFi uit (voor eco-mode)
    void enableWiFi();            // Schakel WiFi in (voor normale werking)
    void handleServerControl();    // Beheer de webserver (voor setup mode)
    void handleWiFiEco();         // Beheer de WiFi eco-mode (aan/uit op basis van tijd)
    // void startDNSServer();        // Start de captive portal DNS server voor setup mode
    // void stopDNSServer();         // Stop de captive portal DNS server  voor normale werking
    // void handleDNSServer();      // Beheer de captive portal DNS server (moet regelmatig worden aangeroepen in loop)
    void activateWiFiAndServer();   // Activeer WiFi en webserver (voor normale werking)
    void deactivateWiFiAndServer(); // Deactiveer WiFi en webserver (voor eco-mode)
    void manageServerTimeout();      // Beheer de timeout voor de webserver (om te voorkomen dat deze te lang actief blijft na setup)

    // paneel_logic.h
    void performTransition();          // Voert een overgang uit tussen twee sprites (voor panelwissels)
    void updateDataPaneelVandaag();     // Update het 'vandaag' data-paneel
    void updateDataPaneelForecast();    // Update het 'forecast' data-paneel
    void showSetupInstructionPanel();   // Toont een instructie-paneel (zoals tijdens setup of als er geen data is)

    // storage_logic.h
    void initStorage();       // Initialiseer de NVS opslag
    void loadUserData();       // Laadt naam en geboortedatum
    void saveDisplaySettings(); // Sla display-instellingen op (zoals helderheid en transitie)
    void loadDisplaySettings(); // Laadt display-instellingen (zoals helderheid en transitie)
    void saveNetworkConfig();   // Sla netwerkconfiguratie op (zoals SSID en wachtwoord)
    void loadNetworkConfig();   // Laadt netwerkconfiguratie (zoals SSID en wachtwoord)
    void saveOMWConfig();       // Sla OWM-configuratie op (zoals API-sleutel en locatie)
    void loadOMWConfig();       // Laadt OWM-configuratie (zoals API-sleutel en locatie)
    void loadWeatherCache();    // Laadt de laatste weerdata uit de cache (voor snelle opstart)

}
