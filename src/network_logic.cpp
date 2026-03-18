/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Network logic: WiFi en OTA setup
 * Hierin staan de functies die te maken hebben met het opzetten van de WiFi-verbinding en het beheren van OTA updates.
 * Ook de eco-mode voor WiFi (automatisch aan/uit op basis van tijd) wordt hier beheerd.
 * het starten van de setup mode (AP + captive portal),
 */

#include "network_logic.h"
#include "bitmaps/BootImages.h"
#include "global_data.h"
#include "helpers.h"
#include "secret.h" // Nodig voor OTA-naam/wachtwoord en OWM data
#include "storage_logic.h"
#include "web_config.h"
// #include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

// Forward declarations
void activateWiFiAndServer();
void deactivateWiFiAndServer();
void enableWiFi();
// void handleTicker();
void handleWiFiEco();
void manageServerTimeout();
void renderTicker();
void setupWiFi();
void startAccessPoint();
void stopSetupMode();
// void setupOTA();
void showNetworkInfo();
// void updateTickerSegments();

void powerDownWiFi();

DNSServer dnsServer;
const byte DNS_PORT = 53;

void powerDownWiFi() {
    // CHECK: Als de modus 0 is (Altijd aan), dan doen we NIETS.
    if (state.network.wifi_mode == 0) {
        state.network.is_updating = false; // Wel de vlag resetten!
        Serial.printf("[NET] WiFi blijft aan (Modus: Altijd aan).\n");
        return; 
    }

    // Voor Modus 1 (On-demand) en Modus 2 (ECO) gaan we wel echt uit:
    if (WiFi.status() == WL_CONNECTED || state.network.web_server_active) {
        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        state.network.web_server_active = false;
        state.network.is_updating = false;
        state.network.wifi_enabled = false;
        Serial.printf("[NET] WiFi fysiek uitgeschakeld (On-demand/ECO).\n");
    }
}

void enableWiFi()
{
    state.network.wifi_enabled = true;
    setupWiFi(); // Onze bestaande functie die de verbinding start
    Serial.printf("[NET] WiFi Radio INGESCHAKELD.\n");
}

// void handleTicker()
// {
//     state.display.ticker_x -= 1;
//     if (state.display.ticker_x < -state.display.total_ticker_width)
//     {
//         state.display.ticker_x = tft.width();
//         updateTickerSegments();
//     }
//     // Teken de sprite op het scherm (over de dolfijn heen!)
//     renderTicker();
// }

void handleWiFiEco()
{
    if (state.network.wifi_mode != 2)
        return;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    int huidigUur = timeinfo.tm_hour;
    static bool wifiIsGepauzeerd = false;

    // Nacht-logica
    bool isNacht = (huidigUur >= 23 || huidigUur < 7);

    // DE SLIMME CHECK:
    // We gaan pas slapen als het nacht is EN:
    // - De data is vers (missie geslaagd)
    // - OF we zijn NIET momenteel aan het updaten (veiligheid)
    bool magSlapen = isNacht && state.weather.data_is_fresh && !state.network.is_updating;

    if (magSlapen && !wifiIsGepauzeerd)
    {
        Serial.printf("[NET] ECO: Data is vers, WiFi mag nu slapen...\n");
        // disableWiFi();
        powerDownWiFi();
        wifiIsGepauzeerd = true;
    }
    // We worden wakker als het geen nacht meer is
    else if (!isNacht && wifiIsGepauzeerd)
    {
        Serial.printf("[NET] ECO: Nacht voorbij, WiFi wordt wakker...\n");
        enableWiFi();
        wifiIsGepauzeerd = false;
        // Optioneel: forceer direct een update na het ontwaken
        Config::forceFirstWeatherUpdate = true;
    }
}

// --- 3. DE HOOFD WIFI FUNCTIE ---
void setupWiFi()
{
    // We halen de verse data uit de state (geladen door initStorage)
    String ssid = state.network.ssid;
    String pass = state.network.pass;

    // 1. Check op geldige gegevens
    if (ssid == "" || ssid == "leeg" || ssid.length() < 2)
    {
        state.network.is_setup_mode = true;
        state.network.server_start_time = millis();

        startAccessPoint(); // Zorg dat deze functie de WiFi in AP mode zet
        Serial.printf("[WIFI] Geen geldige gegevens gevonden. Start Setup Mode...\n");
        // startSetupMode();
        return;
    }

    // 2. Verbinding maken
    Serial.printf("[WIFI] Verbinden met: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        yield(); // Geef de WiFi-stack ademruimte
    }

    // 3. Resultaat afhandelen
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.printf("[WIFI] Verbinding mislukt. Schakel over naar AP...\n");
        state.network.is_setup_mode = true;
        state.network.server_start_time = millis();
        startAccessPoint(); // Zorg dat deze functie de WiFi in AP mode zet
        // startSetupMode();
    }
    else
    {
        Serial.printf("[WIFI] Verbonden!\n");

        // --- S.P.O.T. SYNC ---
        state.network.wifi_connected = true;
        state.network.is_setup_mode = false; // Belangrijk: zet de klok-loop weer 'aan'
        // initWebServer(); // We starten de server pas als we zeker weten dat we verbinding hebben, en met de juiste gegevens
        // setupOTA();
    }

    if (MDNS.begin(state.network.mdns.c_str()))
    {
        Serial.printf("[NET] mDNS actief: http://%s.local\n", state.network.mdns.c_str());
    }
    Serial.printf("[NET] IP-Adres  : %s\n", WiFi.localIP().toString().c_str());
}

// // --- 4. OVER-THE-AIR (OTA) ---
// void setupOTA()
// {
//     ArduinoOTA.setHostname(state.network.mdns.c_str()); // Gebruik de mDNS-naam uit de state
//     // Hier kun je evt. een wachtwoord toevoegen uit secret.h
//     // ArduinoOTA.setPassword(OTA_PASSWORD);

//     ArduinoOTA.onStart([]()
//                        {
//         tft.fillScreen(TFT_BLACK);
//         tft.setTextColor(TFT_GOLD);
//         tft.drawString("OTA Update...", 10, 10); });

//     ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
//                           {
//         int width = (progress / (total / 200));
//         tft.drawRect(20, 150, 200, 10, TFT_WHITE);
//         tft.fillRect(22, 152, width - 4, 6, TFT_GREEN); });

//     // ArduinoOTA.begin();
//     Serial.printf("OTA Ready\n");
// }

void activateWiFiAndServer()
{
    Serial.printf("[NET] Config-modus activeren...\n");

    // 1. Ruim OUDE resten op (Gooi de oude rommel uit de Heap)
    server.end();
    MDNS.end();
    delay(100); // Geef de stack even ademruimte

    state.network.server_start_time = millis(); // De klok begint te tikken

    // 2. Start WiFi (alleen als het echt uit staat)
    if (WiFi.status() != WL_CONNECTED)
    {
        setupWiFi();
    }

    // 1. Zorg dat WiFi aanstaat
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.mode(WIFI_STA);
        WiFi.begin(state.network.ssid, state.network.pass);

        // Wacht max 10 sec (blocking is hier ok, want de gebruiker houdt de knop vast)
        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000)
        {
            delay(100);
            digitalWrite(Config::pin_fingerprint_led, !digitalRead(2)); // Extra feedback tijdens verbinden
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        // 3. Start mDNS met de JUISTE naam uit de state
        if (MDNS.begin(state.network.mdns.c_str()))
        {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("[NET] mDNS: %s.local\n", state.network.mdns.c_str());
            Serial.printf("[NET] IP-Adres: %s\n", WiFi.localIP().toString().c_str());
        }

        // 4. Start de server pas als de rest staat
        initWebServer(); // Zorg dat hier GEEN mDNS.begin meer in staat!
        server.begin();
        state.network.web_server_active = true;
        Serial.printf("[NET] Config-server is nu ACTIEF\n");
    }
}

void manageServerTimeout()
{
    if (state.network.is_setup_mode || state.network.web_server_active)
    {
        if (millis() - state.network.server_start_time >= Config::ten_min_timeout) // 10 minuten
        {
            powerDownWiFi();
        }
    }
}

