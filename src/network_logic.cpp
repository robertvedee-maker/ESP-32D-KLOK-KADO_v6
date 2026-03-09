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
// void disableWiFi();
void enableWiFi();
void handleTicker();
void handleWiFiEco();
void manageServerTimeout();
void renderTicker();
void setupWiFi();
void startAccessPoint();
void stopSetupMode();
// void setupOTA();
void showNetworkInfo();
void updateTickerSegments();

void powerDownWiFi();

DNSServer dnsServer;
const byte DNS_PORT = 53;

void powerDownWiFi() {
    // CHECK: Als de modus 0 is (Altijd aan), dan doen we NIETS.
    if (state.network.wifi_mode == 0) {
        state.network.is_updating = false; // Wel de vlag resetten!
        Serial.println(F("[NET] WiFi blijft aan (Modus: Altijd aan)."));
        return; 
    }

    // Voor Modus 1 (On-demand) en Modus 2 (ECO) gaan we wel echt uit:
    if (WiFi.status() == WL_CONNECTED || state.network.web_server_active) {
        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        state.network.web_server_active = false;
        state.network.is_updating = false;
        Serial.println(F("[NET] WiFi fysiek uitgeschakeld (On-demand/ECO)."));
    }
}
// void powerDownWiFi()
// {
//     if (WiFi.status() == WL_CONNECTED || state.network.web_server_active)
//     {
//         WiFi.softAPdisconnect(true);
//         WiFi.disconnect(true);
//         WiFi.mode(WIFI_OFF); // Radio echt uit
//         state.network.web_server_active = false;
//         state.network.is_updating = false;
//         state.network.wifi_enabled = false;
//         Serial.println(F("[NET] WiFi fysiek uitgeschakeld (Silent)."));
//     }
// }

// void disableWiFi()
// {
//     WiFi.disconnect(true);
//     WiFi.mode(WIFI_OFF);
//     state.network.wifi_connected = false;
//     state.network.wifi_enabled = false;
//     Serial.println(F("[NET] WiFi Radio UITGESCHAKELD (Energiebesparing)."));
// }

void enableWiFi()
{
    state.network.wifi_enabled = true;
    setupWiFi(); // Onze bestaande functie die de verbinding start
    Serial.println(F("[NET] WiFi Radio INGESCHAKELD."));
}

void handleTicker()
{
    state.display.ticker_x -= 1;
    if (state.display.ticker_x < -state.display.total_ticker_width)
    {
        state.display.ticker_x = tft.width();
        updateTickerSegments();
    }
    // Teken de sprite op het scherm (over de dolfijn heen!)
    renderTicker();
}

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
        Serial.println(F("[NET] ECO: Data is vers, WiFi mag nu slapen..."));
        // disableWiFi();
        powerDownWiFi();
        wifiIsGepauzeerd = true;
    }
    // We worden wakker als het geen nacht meer is
    else if (!isNacht && wifiIsGepauzeerd)
    {
        Serial.println(F("[NET] ECO: Nacht voorbij, WiFi wordt wakker..."));
        enableWiFi();
        wifiIsGepauzeerd = false;
        // Optioneel: forceer direct een update na het ontwaken
        Config::forceFirstWeatherUpdate = true;
    }
}

// void handleWiFiEco()
// {
//     // We bemoeien ons er alleen mee als de modus op 2 (ECO/Nacht) staat
//     if (state.network.wifi_mode != 2)
//         return;

//     struct tm timeinfo;
//     if (!getLocalTime(&timeinfo))
//         return;

//     int huidigUur = timeinfo.tm_hour;
//     static bool wifiIsGepauzeerd = false;

//     // Nacht-logica (23:00 - 07:00)
//     bool isNacht = (huidigUur >= 23 || huidigUur < 7);

//     if (isNacht && !wifiIsGepauzeerd)
//     {
//         Serial.println(F("[NET] Nacht-modus: WiFi gaat slapen..."));
//         disableWiFi(); // Gebruik je eigen functie voor fysieke uitschakeling
//         wifiIsGepauzeerd = true;
//     }
//     else if (!isNacht && wifiIsGepauzeerd)
//     {
//         Serial.println(F("[NET] Nacht voorbij: WiFi herstellen..."));
//         enableWiFi();
//         wifiIsGepauzeerd = false;
//     }
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
        state.network.is_setup_mode = true;
        state.network.server_start_time = millis();

        startAccessPoint(); // Zorg dat deze functie de WiFi in AP mode zet
        Serial.println(F("[WIFI] Geen geldige gegevens gevonden. Start Setup Mode..."));
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
        //     // DIT IS DE MAGIE: Terwijl we wachten op WiFi, laten we de Ticker rollen!
        // handleTicker(); // Zorg dat deze functie tckSpr.pushSprite(0, Config::ticker_y) doet
        yield(); // Geef de WiFi-stack ademruimte
        // delay(500);
        // Serial.print(".");
    }

    // 3. Resultaat afhandelen
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(F("\n[WIFI] Verbinding mislukt. Schakel over naar AP..."));
        state.network.is_setup_mode = true;
        state.network.server_start_time = millis();
        startAccessPoint(); // Zorg dat deze functie de WiFi in AP mode zet
        // startSetupMode();
    }
    else
    {
        Serial.println(F("\n[WIFI] Verbonden!"));

        // --- S.P.O.T. SYNC ---
        state.network.wifi_connected = true;
        showNetworkInfo(); // Laat het IP-adres kort op het scherm zien

        state.network.is_setup_mode = false; // Belangrijk: zet de klok-loop weer 'aan'
        initWebServer();
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
//     Serial.println(F("OTA Ready"));
// }

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

void manageServerTimeout()
{
    if (state.network.is_setup_mode || state.network.web_server_active)
    {
        if (millis() - state.network.server_start_time >= Config::ten_min_timeout) // 10 minuten
        {
            // deactivateWiFiAndServer();
            powerDownWiFi();
            // finalizeUIAfterSetup();
            // state.network.is_setup_mode = false;
        }
    }
}

// void deactivateWiFiAndServer()
// {
//     Serial.println(F("[NET] Systeem herstellen naar normale modus..."));

//     // 1. Stop de actieve netwerkservices
//     server.end();
//     MDNS.end();
//     WiFi.softAPdisconnect(true); // Sluit het Access Point en verwijder de SSID uit de lucht
//     stopSetupMode(); // Zorg dat deze functie alle setup-gerelateerde flags reset

//     // 2. Reset ALLE status-vlaggen
//     state.network.web_server_active = false;
//     state.network.is_setup_mode = false; // Cruciaal: hiermee verlaat de loop() het setup-blok
//     state.display.show_config_qr = false;
//     state.network.server_start_time = 0; // Reset de timer

//     // 3. Forceer een UI-verversing
//     // We wissen het scherm om resten van de setup-pagina (QR, Goud/Groen kader) te verwijderen
//     tft.fillScreen(TFT_BLACK);
//     state.display.force_ticker_refresh = true;
//     state.display.force_alert_display = false; // Stop eventuele geforceerde meldingen

//     // 4. Visuele & Seriële feedback
//     Serial.println(F("[SYSTEM] Webserver gesloten. Terug naar Ticker/Klok."));

//     // Knipper de LED 3x traag als bevestiging van afsluiten
//     for (int i = 0; i < 3; i++)
//     {
//         digitalWrite(Config::pin_fingerprint_led, HIGH);
//         delay(150);
//         digitalWrite(Config::pin_fingerprint_led, LOW);
//         delay(150);
//     }

//     // 5. Herstel WiFi-verbinding (indien nodig)
//     // Als de klok normaal op je thuis-WiFi draait, zorgen we dat hij weer verbindt
//     if (WiFi.status() != WL_CONNECTED && state.network.ssid.length() > 2)
//     {
//         Serial.println(F("[NET] WiFi verbinding herstellen..."));
//         WiFi.begin(state.network.ssid.c_str(), state.network.pass.c_str());
//     }
// }
