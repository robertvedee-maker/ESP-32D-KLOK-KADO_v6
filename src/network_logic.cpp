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

// Forward declarations
void renderTicker();
void updateTickerSegments();
// void drawSetupModeActive();
void showNetworkInfo();

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

void handleTicker() {
    state.display.ticker_x -= 1;
    if (state.display.ticker_x < -state.display.total_ticker_width) {
        state.display.ticker_x = tft.width();
        updateTickerSegments();
    }
    // Teken de sprite op het scherm (over de dolfijn heen!)
    renderTicker(); 
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

// // --- 2. DE SETUP MODUS (Als WiFi faalt) ---
// void startSetupMode()
// {
//     state.network.is_setup_mode = true; // De kraan gaat dicht voor de rest

//     drawSetupModeActive();

//     // Wifi Access Point starten
//     WiFi.mode(WIFI_AP);

//     IPAddress apIP(192, 168, 4, 1);
//     WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

//     WiFi.softAP(SECRET_AP_SSID, SECRET_AP_PASSWORD);

//     MDNS.begin(SECRET_AP_SSID); // Bereikbaar via http://xxx.local

//     // Wacht even tot de WiFi-stack gesetteld is
//     delay(2000);

//     initWebServer();

//     Serial.println(F("Setup Mode gestart op 192.168.4.1"));

// // // Hoe je dit "Live" maakt:
// // // In je setup() of loop() waar je de DNS Server en SoftAP afhandelt voor de config-modus, moet je deze functie af en toe verversen.
// // // dit is alvast een voorbereiding voor de S3. in de 32D is de webconfig alleen beschikbaar via 5 seconde touch op de ladder, 
// // // dus we hoeven hier nog geen timer of zo te maken. 
// // // We checken gewoon in de loop() of er een verandering is in het aantal verbonden stations, en als dat zo is, 
// // // roepen we drawSetupModeActive() aan om het scherm te verversen.

// // // In de Setup-Modus loop:
// // static int lastStationCount = -1;
// // int currentStations = WiFi.softAPgetStationNum();

// // if (currentStations != lastStationCount) {
// //     drawSetupModeActive(); // Herteken het scherm als er iemand bijkomt of weggaat
// //     lastStationCount = currentStations;
// // }


// }

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
            // DIT IS DE MAGIE: Terwijl we wachten op WiFi, laten we de Ticker rollen!
        handleTicker(); // Zorg dat deze functie tckSpr.pushSprite(0, Config::ticker_y) doet
        yield();        // Geef de WiFi-stack ademruimte
        // delay(500);
        // Serial.print(".");
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
        showNetworkInfo(); // Laat het IP-adres kort op het scherm zien

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
            digitalWrite(Config::pin_fingerprint_led, !digitalRead(2)); // Extra feedback tijdens verbinden
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        // 3. Start mDNS met de JUISTE naam uit de state
        if (MDNS.begin(state.network.mdns.c_str()))
        {
            MDNS.addService("http", "tcp", 80);
            Serial.print(F("[NET] mDNS: "));
            Serial.print(state.network.mdns);
            Serial.println(F(".local"));
            Serial.print(F("[NET] IP-Adres: "));
            Serial.println(WiFi.localIP().toString());
        }

        // 4. Start de server pas als de rest staat
        initWebServer(); // Zorg dat hier GEEN mDNS.begin meer in staat!
        server.begin();
        state.network.web_server_active = true;
        Serial.println(F("[NET] Config-server is nu ACTIEF"));
    }
}

void manageServerTimeout() {
    if (state.network.web_server_active) {
        if (millis() - state.network.server_start_time >= 600000UL) {
            deactivateWiFiAndServer();
        }
    }
}

void deactivateWiFiAndServer() 
{
    Serial.println(F("[NET] Systeem herstellen naar normale modus..."));

    // 1. Stop de actieve netwerkservices
    server.end();
    MDNS.end();
    WiFi.softAPdisconnect(true); // Sluit het Access Point en verwijder de SSID uit de lucht
    
    // 2. Reset ALLE status-vlaggen
    state.network.web_server_active = false;
    state.network.is_setup_mode = false;     // Cruciaal: hiermee verlaat de loop() het setup-blok
    state.display.show_config_qr = false;
    state.network.server_start_time = 0;     // Reset de timer
    
    // 3. Forceer een UI-verversing
    // We wissen het scherm om resten van de setup-pagina (QR, Goud/Groen kader) te verwijderen
    tft.fillScreen(TFT_BLACK); 
    state.display.force_ticker_refresh = true;
    state.display.force_alert_display = false; // Stop eventuele geforceerde meldingen

    // 4. Visuele & Seriële feedback
    Serial.println(F("[SYSTEM] Webserver gesloten. Terug naar Ticker/Klok."));
    
    // Knipper de LED 3x traag als bevestiging van afsluiten
    for (int i = 0; i < 3; i++) {
        digitalWrite(Config::pin_fingerprint_led, HIGH);
        delay(150);
        digitalWrite(Config::pin_fingerprint_led, LOW);
        delay(150);
    }

    // 5. Herstel WiFi-verbinding (indien nodig)
    // Als de klok normaal op je thuis-WiFi draait, zorgen we dat hij weer verbindt
    if (WiFi.status() != WL_CONNECTED && state.network.ssid.length() > 2) {
        Serial.println(F("[NET] WiFi verbinding herstellen..."));
        WiFi.begin(state.network.ssid.c_str(), state.network.pass.c_str());
    }
}
