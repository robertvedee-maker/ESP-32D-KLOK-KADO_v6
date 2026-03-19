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

// Forward declaration: We vertellen de compiler alleen DAT er een 
// klasse bestaat die TFT_eSprite heet, zonder de hele library te laden.
class TFT_eSprite; 

// 1. De centrale 'state' struct die de huidige status van de app bijhoudt (zoals sensorwaarden, netwerkstatus, display-instellingen, etc.)
// #include "clasic_clock.h"  // Voor de klokdata

// #include "daynight.h"      // Voor tijd en helderheid
// #include "display_logic.h" // Voor ticker en panelen
// #include "env_sensors.h"   // Voor BME/AHT sensoren

// #include "helpers.h"       // Voor setupDisplay, updateClock, etc.
// #include "leeftijd_calc.h" // Voor het berekenen van leeftijd en ongewone weetjes op basis van geboortedatum
// #include "network_logic.h" // Voor WiFi en OTA setup

// #include "storage_logic.h" // Voor NVS opslag
// #include "weather_logic.h" // Voor OWM data
#include "web_config.h"    // Voor de webserver en captive portal tijdens setup mode
#include "config.h"        // Voor pinnen en constanten
#include "global_data.h"   // Voor toegang tot 'state'
#include "secret.h"        // Voor WiFi credentials
#include "driver/ledc.h" // Voor PWM controle van de backlight



#include "bitmaps/weatherIcons40.h" // Voor de kleine weer-icoontjes in het 'vandaag' paneel
#include "bitmaps/weatherIcons68.h" // Voor de grotere weer-icoontjes in het 'forecast' paneel
#include "bitmaps/BootImages.h"     // Voor de opstart- en foutafbeeldingen

// 2. De functies die in de loop() en setup() worden aangeroepen, zodat we een centrale plek hebben voor alle 'acties' die de app kan uitvoeren.
#include <Arduino.h>
#include <ArduinoJson.h>
// #include <ArduinoOTA.h>
#include <Preferences.h>
#include <time.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <cstdint>
#include <vector>
#include <LittleFS.h>
#include <qrcode.h>
#include <algorithm>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// // 1. Hardware instanties (MOETEN hier gedefinieerd worden)
// TFT_eSPI tft = TFT_eSPI();
// TFT_eSprite clkSpr = TFT_eSprite(&tft);
// TFT_eSprite datSpr1 = TFT_eSprite(&tft);
// TFT_eSprite datSpr2 = TFT_eSprite(&tft);
// TFT_eSprite datSpr3 = TFT_eSprite(&tft);
// TFT_eSprite tckSpr = TFT_eSprite(&tft);

namespace App
{
    // birthday_logic.cpp
    void saveBirthdays(String rawText);
    void updateBirthdayList();
    void updateDailyBirthdayState();

    String getBirthdaysRaw();
    std::vector<BirthdayEntry> getSortedBirthdays(int limit); // Voor toegang tot de gesorteerde verjaardagslijst, bijvoorbeeld in de display logic
    extern std::vector<ActiveBirthday> currentBirthdays;             // We slaan alleen de verjaardag van 'vandaag' of 'binnenkort' op in RAM

    // daynight.cpp
    void updateDateTimeStrings();
    void manageTimeFunctions();
    void manageBrightness();

    // leeftijd_calc.cpp
    void vulEasterEggTekst(char *buffer, size_t bufferSize, int gDag, int gMaand, int gJaar, int forceVariant);
    bool vulGepersonaliseerdFeitje(char *buffer, size_t bufferSize);

    // network_logic.cpp
    void activateWiFiAndServer();
    void powerDownWiFi();
    void enableWiFi();
    void handleWiFiEco();
    void setupWiFi();
    void startAccessPoint();
    void stopSetupMode();

    // setup_manager.cpp
    void drawMonitorWifi(int x, int y, int h);
    void drawMonitorAlert(int x, int y, int size, uint16_t color, bool unused);
    void drawSetupModeActive();
    void drawSystemMonitor();
    void haltSystemWithInstruction();
    void showNetworkInfo();
    void showSetupInstructionPanel();

    //weather_logic.cpp
    void manageWeatherUpdates();
    bool fetchWeather(bool checkOnly = false);
    // void saveWeatherCache();
    // void loadWeatherCache();
    bool forceFirstWeatherUpdate();

    // env_sensors.cpp
    bool setupSensors();
    void handleSensors();
    void updateBaroTrend();
    // void manageSensorUpdates();
    // void readBME280();
    // void readAHT20();
    // void readDS18B20();


    // display_logic.cpp - helpers.cpp

    void vulBootSegmenten();
    void vulAlertSegmenten();
    void vulNormalSegmenten();
    void addSegment(String t, uint16_t c);
    void updateTickerSegments();
    void renderTicker();
    void performTransition(TFT_eSprite *oldSpr, TFT_eSprite *newSpr);
    void manageDataPanels();
    void manageEasterEggTimer();
    void manageAlertTimeout();
    void manageServerTimeout();
    void manageStatusLed();
    void manageTimeFunctions();
    void makeUpperCase(char *str);
    int berekenMinutenTotUpdate();
    void evaluateSystemSafety();
    void finalizeUIAfterSetup();
    void updateTouchLedFeedback(unsigned long duration);

    void updateBirthdayAlertState();
    void updateDataPaneelAlert();
    void updateDataPaneelVandaag();
    void updateDataPaneelForecast();
    void updateDisplayBrightness(int level);
    void updateTouchLedFeedback(unsigned long duration);

    void vulEasterEggTekst(char *buffer, size_t bufferSize, int gDag, int gMaand, int gJaar, int forceVariant);
    bool vulGepersonaliseerdFeitje(char *buffer, size_t bufferSize);

    void drawVerjaardagsKalender(TFT_eSprite &spr);
    void drawPartyPopper(TFT_eSprite &spr, int x, int y, char gender);
    void drawWeatherIcon(TFT_eSprite &spr, int x, int y, int size, int iconId, bool isDay);
    void drawISOAlert(int x, int y, int size, uint16_t color, bool active);
    void drawWifiIndicator(int x, int y, int h);
    void drawQRCodeOnTFT(const char *data, int x, int y, int scale);

    void finalizeSetupAndShowDashboard();
    void addTickerSegment(String txt, uint16_t col);
    void handleHardware();
    void handleTouchLadder();
    void setupDisplay();
    void updateClock();

    // String getWindRoos(int graden);
    // int getBeaufort(float ms);
    // uint16_t getIconColor(int conditionCode);


    


}
