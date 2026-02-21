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
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

DNSServer dnsServer;
const byte DNS_PORT = 53;

void disableWiFi()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    state.network.wifi_connected = false;
    state.network.wifi_enabled = false;
    Serial.println(F("[NET] WiFi Radio fysiek UITGESCHAKELD (Energiebesparing)."));
}

void enableWiFi()
{
    state.network.wifi_enabled = true;
    setupWiFi(); // Onze bestaande functie die de verbinding start
    Serial.println(F("[NET] WiFi Radio INGESCHAKELD."));
}

void handleWiFiEco()
{
    // We bemoeien ons er alleen mee als de modus op 2 (ECO/Nacht) staat
    if (state.network.wifi_mode != 2)
        return;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    int huidigUur = timeinfo.tm_hour;
    static bool wifiIsGepauzeerd = false;

    // Nacht-logica (23:00 - 07:00)
    bool isNacht = (huidigUur >= 23 || huidigUur < 7);

    if (isNacht && !wifiIsGepauzeerd)
    {
        Serial.println(F("[NET] Nacht-modus: WiFi gaat slapen..."));
        disableWiFi(); // Gebruik je eigen functie voor fysieke uitschakeling
        wifiIsGepauzeerd = true;
    }
    else if (!isNacht && wifiIsGepauzeerd)
    {
        Serial.println(F("[NET] Nacht voorbij: WiFi herstellen..."));
        enableWiFi();
        wifiIsGepauzeerd = false;
    }
}

// --- 2. DE SETUP MODUS (Als WiFi faalt) ---
void startSetupMode()
{
    state.network.is_setup_mode = true; // De kraan gaat dicht voor de rest

    drawSetupModeActive();

    // Wifi Access Point starten
    WiFi.mode(WIFI_AP);

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    WiFi.softAP(SECRET_AP_SSID, SECRET_AP_PASSWORD);

    MDNS.begin(SECRET_AP_SSID); // Bereikbaar via http://xxx.local

    // Wacht even tot de WiFi-stack gesetteld is
    delay(2000);

    initWebServer();

    Serial.println(F("Setup Mode gestart op 192.168.4.1"));
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
        Serial.println(F("[WIFI] Geen geldige gegevens gevonden. Start Setup Mode..."));
        startSetupMode();
        return;
    }

    // 2. Verbinding maken
    Serial.printf("[WIFI] Verbinden met: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        delay(500);
        Serial.print(".");
    }

    // 3. Resultaat afhandelen
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(F("\n[WIFI] Verbinding mislukt. Schakel over naar AP..."));
        startSetupMode();
    }
    else
    {
        Serial.println(F("\n[WIFI] Verbonden!"));

        // --- S.P.O.T. SYNC ---
        state.network.wifi_connected = true;
        toonNetwerkInfo(); // Laat het IP-adres kort op het scherm zien

        state.network.is_setup_mode = false; // Belangrijk: zet de klok-loop weer 'aan'
        // initWebServer();
        // setupOTA();
    }

    if (MDNS.begin(state.network.mdns.c_str()))
    {
        Serial.print(F("[NET] mDNS actief: http://"));
        Serial.print(state.network.mdns);
        Serial.println(F(".local"));
    }
    Serial.print(F("[NET] IP-Adres  : "));
    Serial.println(WiFi.localIP());
}

// --- 4. OVER-THE-AIR (OTA) ---
void setupOTA()
{
    ArduinoOTA.setHostname(state.network.mdns.c_str()); // Gebruik de mDNS-naam uit de state
    // Hier kun je evt. een wachtwoord toevoegen uit secret.h
    // ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]()
                       {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GOLD);
        tft.drawString("OTA Update...", 10, 10); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
        int width = (progress / (total / 200));
        tft.drawRect(20, 150, 200, 10, TFT_WHITE);
        tft.fillRect(22, 152, width - 4, 6, TFT_GREEN); });

    // ArduinoOTA.begin();
    Serial.println(F("OTA Ready"));
}

void handleServerControl()
{
    int touchVal = touchRead(13);
    static unsigned long touchStart = 0;
    static bool isTouching = false;
    static bool actieBevestigd = false;

    if (touchVal < 60)
    {
        if (!isTouching)
        {
            touchStart = millis();
            isTouching = true;
            actieBevestigd = false;
        }

        unsigned long duur = millis() - touchStart;

        if (!actieBevestigd)
        {
            if (duur < 5000)
            {
                // HET CREATIEVE PATROON (Feedback)
                int interval = map(duur, 0, 5000, 400, 80);
                digitalWrite(2, (millis() % (interval * 2) < interval));
            }
            else
            {
                // DE FINALE: Schakel de server
                state.network.web_server_active = !state.network.web_server_active;

                if (state.network.web_server_active)
                {
                    server.begin(); // WEK DE REUS
                    Serial.println(F("[WEB] Server ACTIEF"));
                }
                else
                {
                    server.end(); // RUST IN HET SYSTEEM
                    Serial.println(F("[WEB] Server INACTIEF"));
                }

                // Bevestigings-flits (zonder delay voor vloeiende klok)
                for (int f = 0; f < 6; f++)
                { // 6 wisselingen = 3 flitsen
                    digitalWrite(2, !digitalRead(2));
                    // Een micro-pauze die de SPI-bus niet blokkeert
                    unsigned long wait = millis();
                    while (millis() - wait < 40)
                    {
                        yield();
                    }
                }

                actieBevestigd = true;
            }
        }
    }
    else
    {
        if (isTouching)
        {
            isTouching = false;
            // Als de server uit is, moet de LED ook echt uit
            if (!state.network.web_server_active)
                digitalWrite(2, LOW);
        }
    }

    // STATUS LED: 'Ademend' patroon als de server aan staat
    if (state.network.web_server_active && !isTouching)
    {
        // Elke 5 sec een korte onderbreking (ademhaling van de server)
        digitalWrite(2, (millis() % 5000 > 150));
    }
}

void activateWiFiAndServer()
{
    Serial.println(F("[NET] Config-modus activeren..."));

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
            digitalWrite(2, !digitalRead(2)); // Extra feedback tijdens verbinden
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        // 3. Start mDNS met de JUISTE naam uit de state
        if (MDNS.begin(state.network.mdns.c_str()))
        {
            MDNS.addService("http", "tcp", 80);
            Serial.println("[NET] mDNS: " + state.network.mdns + ".local");
        }

        // 4. Start de server pas als de rest staat
        initWebServer(); // Zorg dat hier GEEN mDNS.begin meer in staat!
        server.begin();
        state.network.web_server_active = true;
    }
}

void manageServerTimeout()
{
    if (state.network.web_server_active)
    {

        // 600.000 ms = 10 minuten
        if (millis() - state.network.server_start_time >= 600000)
        {

            Serial.println(F("[NET] 10 minuten voorbij. Server wordt gesloten..."));

            server.end();
            MDNS.end();
            state.network.web_server_active = false;

            // Optioneel: Knipper de LED 5x als afscheid
            for (int i = 0; i < 5; i++)
            {
                digitalWrite(2, !digitalRead(2));
                delay(50);
            }
            digitalWrite(2, LOW);
        }
    }
}

void deactivateWiFiAndServer()
{
    server.end();
    state.network.web_server_active = false;
    // Optioneel: WiFi weer naar laag verbruik als je geen updates nodig hebt
    // WiFi.disconnect();
    // WiFi.mode(WIFI_OFF);
    digitalWrite(2, LOW);
    Serial.println(F("[NET] Server offline"));
}
