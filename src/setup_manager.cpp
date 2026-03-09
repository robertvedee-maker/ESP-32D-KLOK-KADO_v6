#include "global_data.h"
#include "config.h"
#include "helpers.h"
#include "bitmaps/BootImages.h"

#include "secret.h"
#include "web_config.h"
#include <ESPmDNS.h>
#include "storage_logic.h"
#include <Preferences.h>

#include "Setup_Manager.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <TFT_eSPI.h>

extern AsyncWebServer server;
extern void drawQRCodeOnTFT(const char *data, int x, int y, int scale);
void drawSetupScreen();
void drawSetupModeActive();
void haltSystemWithInstruction();
// void startSetupMode();
// void runSetupCycle();
void startAccessPoint();
extern void updateDateTimeStrings();

// Interne variabelen (alleen zichtbaar in dit bestand)
static unsigned long lastRedraw = 0;
const unsigned long REDRAW_INTERVAL = 1000; // 1 FPS is genoeg voor setup

static bool monitorBackgroundDrawn = false;
// 1. STATUS BIJHOUDEN (De 'Grendel')
// We gebruiken -1 als startwaarde zodat hij de eerste keer altijd tekent
static int lastStatus = -1;

// Lokale hulpfunctie voor het WiFi icoon op de monitor (Zonder Sprite overhead!)
void drawMonitorWifi(int x, int y, int h)
{
    int bars = 0;
    int rssi = WiFi.RSSI();
    if (WiFi.status() != WL_CONNECTED)
        bars = 0;
    else if (rssi > -55)
        bars = 4;
    else if (rssi > -70)
        bars = 3;
    else if (rssi > -85)
        bars = 2;
    else if (rssi > -98)
        bars = 1;

    for (int i = 0; i < 4; i++)
    {
        uint16_t color = (i < bars) ? TFT_SKYBLUE : TFT_DARKGREY;
        tft.fillRoundRect(x + (i * 6), y + (h - (i * 4) - 4), 4, (i * 4) + 4, 1, color);
    }
}

// Lokale hulpfunctie voor het Alert icoon (Rechtstreeks op TFT)
void drawMonitorAlert(int x, int y, int size, uint16_t color = 0, bool unused = false)
{
    // Basis driehoek
    tft.fillTriangle(x + size / 2, y, x, y + size, x + size, y + size, color);
    // Uitroepteken in zwart
    int bw = size / 6;
    int bh = size / 2.2;
    tft.fillRoundRect(x + (size / 2) - (bw / 2), y + (size / 3), bw, bh, 1, TFT_BLACK);
    tft.fillCircle(x + (size / 2), y + size - (size / 4), size / 10, TFT_BLACK);
}

// --- PRIVATE HULPFUNCTIES (Niet in .h zetten) ---

static void drawStep(const char *title, const char *l1, const char *l2, uint16_t color)
{
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(2, 2, 316, 166, 10, color);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextFont(2);
    tft.drawString(title, 15, 80);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(l1, 15, 100);
    tft.drawString(l2, 15, 120);
}

// --- PUBLIEKE FUNCTIES ---

void startAccessPoint()
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

void drawSetupModeActive()
{
    // Bepaal de huidige situatie
    int currentStatus;
    if (state.network.web_server_active && WiFi.status() == WL_CONNECTED)
    {
        currentStatus = 2; // Scenario A: Beheer (via eigen WiFi)
    }
    else
    {
        currentStatus = (WiFi.softAPgetStationNum() > 0) ? 1 : 0; // Scenario B: Stap 1 of 2
    }
    // static int lastStatus = -1;
    // int currentStatus;

    // // Pak de actuele gegevens
    // int stations = WiFi.softAPgetStationNum();
    // bool isConnectedSTA = (WiFi.status() == WL_CONNECTED);
    // bool isServerActive = state.network.web_server_active;

    // // Bepaal de status nauwkeuriger
    // if (isServerActive && isConnectedSTA)
    // {
    //     currentStatus = 2; // BEHEER (via eigen router)
    // }
    // else if (stations > 0)
    // {
    //     currentStatus = 1; // STAP 2 (Telefoon verbonden met AP)
    // }
    // else
    // {
    //     currentStatus = 0; // STAP 1 (AP zoekt verbinding)
    // }

    // 2. HET STATISCHE DEEL (Alleen tekenen bij verandering van status!)
    if (currentStatus != lastStatus)
    {
        tft.fillScreen(TFT_BLACK);
        tft.drawRoundRect(2, 2, 316, 166, 10, TFT_GOLD);
        tft.drawBitmap(15, 15, image_DolphinWait_bits, 59, 54, TFT_WHITE);
        tft.setTextFont(2);

        char url[50];

        if (currentStatus == 2)
        { // BEHEER MODUS
            tft.setTextColor(TFT_GOLD);
            tft.drawString("BEHEER MODUS ACTIEF", 15, 80);
            tft.setTextColor(TFT_WHITE);
            tft.drawString("Pas instellingen aan via:", 15, 100);
            snprintf(url, sizeof(url), "http://%s", WiFi.localIP().toString().c_str());
            tft.setTextColor(TFT_SKYBLUE);
            tft.drawString(url, 15, 120);
            drawQRCodeOnTFT(url, 195, 35, 3);
        }
        else if (currentStatus == 0)
        { // STAP 1: AP ZOEKT
            tft.setTextColor(TFT_GOLD);
            tft.drawString("STAP 1: VERBINDEN", 15, 80);
            tft.setTextColor(TFT_WHITE);
            tft.drawString("Scan WiFi-netwerk:", 15, 100);
            tft.setTextColor(TFT_SKYBLUE);
            tft.drawString("SSID: Kado-Klok-Setup", 15, 120);
            drawQRCodeOnTFT("WIFI:S:Kado-Klok-Setup;T:nopass;;", 195, 35, 3);
        }
        else
        { // STAP 2: AP VERBONDEN
            tft.setTextColor(TFT_GREEN);
            tft.drawString("STAP 2: CONFIGUREREN", 15, 80);
            tft.setTextColor(TFT_WHITE);
            tft.drawString("Verbonden! Ga naar:", 15, 100);
            snprintf(url, sizeof(url), "http://192.168.4.1");
            // snprintf(url, sizeof(url), "http://%s", WiFi.softAPIP().toString().c_str());
            tft.setTextColor(TFT_SKYBLUE);
            tft.drawString(url, 15, 120);
            drawQRCodeOnTFT(url, 195, 35, 3);
        }

        tft.setTextFont(1);
        tft.setTextColor(TFT_LIGHTGREY);
        tft.drawCentreString("SCAN MIJ", 245, 145, 1);

        lastStatus = currentStatus; // Zet de grendel vast
        Serial.printf("[DISPLAY] Setup status gewisseld naar: %d\n", currentStatus);
    }

    // 3. HET DYNAMISCHE DEEL (Dit tekent ELKE keer, want dit knippert niet)
    int xWidth = tft.width() - 20;
    unsigned long elapsed = millis() - state.network.server_start_time;
    unsigned long timeout = Config::ten_min_timeout;
    int barWidth = map(elapsed, 0, timeout, xWidth, 0);
    barWidth = constrain(barWidth, 0, xWidth);

    tft.drawRect(10, 160, xWidth, 5, TFT_DARKGREY);
    tft.fillRect(10, 160, barWidth, 5, TFT_GOLD);
    tft.fillRect(barWidth + 11, 161, xWidth - barWidth - 2, 3, TFT_BLACK);

    tft.fillCircle(158, 10, 4, state.display.touch_indicator_color);
}

void drawSystemMonitor()
{
    // 1. EENMALIGE BASIS (Het kader en de labels)
    if (!monitorBackgroundDrawn)
    {
        tft.fillScreen(TFT_BLACK);
        tft.drawFastHLine(5, 25, 310, TFT_DARKGREY);
        tft.drawFastHLine(5, 140, 310, TFT_DARKGREY);

        tft.setTextFont(2);
        tft.setTextColor(TFT_LIGHTGREY);
        tft.drawString("Heap:", 15, 60);
        tft.drawString("MaxBlok:", 15, 80);
        tft.drawString("Temp:", 15, 100);
        tft.drawString("Sensoren:", 15, 120);

        tft.setTextFont(1);
        tft.drawCentreString("HOLD 3s TO EXIT", 260, 150, 1);
        monitorBackgroundDrawn = true;
    }

    // 2. DYNAMISCHE DATA (Met achtergrondkleur om flikkeren te voorkomen)
    updateDateTimeStrings();

    tft.setTextFont(2);

    // Bovenbalk: Tijd & Datum
    // Door TFT_BLACK als tweede parameter te geven, wist de tekst zichzelf
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    String topRow = String(state.env.current_time_str) + "  |  " + String(state.env.current_date_str);
    tft.drawString(topRow.c_str(), 10, 5);

    tft.fillCircle(280, 8, 4, state.display.touch_indicator_color); // Touch feedback indicator

    drawMonitorWifi(295, 5, 15);

    // Status melding
    if (state.env.health != HEALTH_OK)
    {
        drawMonitorAlert(10, 35, 40, state.env.get_current_alert_color(), true);
        tft.setTextColor(state.env.get_current_alert_color(), TFT_BLACK);
        tft.drawString("SYSTEEM ALERT!          ", 60, 45); // Spaties wissen oude tekst
    }
    else
    {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("SYSTEEM STATUS: OPTIMAAL", 10, 35);
    }

    // Variabelen voor actuele metingen
    char heapBuffer[32];
    char maxAllocBuffer[32];
    char tempBuffer[32];
    char sensorBuffer[32];

    // We voegen de actuele metingen per onderdeel samen in één string
    snprintf(heapBuffer, sizeof(heapBuffer), "%dkB (%dkB min)   ", ESP.getFreeHeap() / 1024, ESP.getMinFreeHeap() / 1024);
    snprintf(maxAllocBuffer, sizeof(maxAllocBuffer), "%dkB   ", ESP.getMaxAllocHeap() / 1024);
    snprintf(tempBuffer, sizeof(tempBuffer), "%.0fC (max %.0fC)  ", state.env.case_temp, Config::system_temp_warning);
    snprintf(sensorBuffer, sizeof(sensorBuffer), "AHT: %.1fC (%.1fC) BMP: %.1f hPa  ", state.env.temp_local, state.env.raw_temp_local, state.env.press_local);

    tft.setTextColor(TFT_WHITE, TFT_BLACK); // De zwarte achtergrond wast de oude tekst weg
    tft.setTextFont(2);
    tft.drawString(heapBuffer, 85, 60); // Teken de hele handel op een vaste positie

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(maxAllocBuffer, 85, 80); // Teken de hele handel op een vaste positie

    tft.setTextColor(state.env.health == HEALTH_OK ? TFT_WHITE : TFT_ORANGE, TFT_BLACK);
    tft.drawString(tempBuffer, 85, 100); // Teken de hele handel op een vaste positie

    if (state.env.aht_ok && state.env.bmp_ok)
    {
        tft.setTextColor(state.env.health == HEALTH_OK ? TFT_WHITE : TFT_ORANGE, TFT_BLACK);
        tft.drawString(sensorBuffer, 85, 120); // Teken de hele handel op een vaste positie
    }
    else
    {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("SENSOR FOUT!               ", 85, 120); // Spaties wissen oude tekst
    }

    // Uptime (onderaan)
    tft.setTextFont(1);
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    char uptimeStr[32];
    uint32_t sec = millis() / 1000;
    snprintf(uptimeStr, sizeof(uptimeStr), "UPTIME: %02du %02dm %02ds", (sec / 3600), (sec % 3600) / 60, sec % 60);
    tft.drawString(uptimeStr, 15, 150);
}

void haltSystemWithInstruction()
{
    // 1. Scherm op zwart en kader tekenen
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(2, 2, 316, 166, 10, TFT_RED);
    tft.drawBitmap(15, 15, image_DolphinReceive_bits, 97, 61, TFT_WHITE);

    // 2. De instructie
    tft.setTextColor(TFT_ORANGE);
    tft.drawCentreString("SETUP TIMEOUT", 180, 56, 4);

    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Geen WiFi-gegevens ontvangen.", 160, 84, 2);
    tft.setTextColor(TFT_GOLD);
    tft.drawCentreString("HERSTART DE KLOK:", 160, 110, 2);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("Haal de stekker eruit en er weer in.", 160, 135, 1);

    Serial.println(F("[SYSTEM] Systeem bevroren. Wacht op power-cycle..."));

    // 3. De 'Freeze'
    // We laten de Buck aan (digitalWrite LOW), anders zie je de tekst niet.
    while (true)
    {
        yield(); // Voorkom dat de Watchdog de ESP herstart
        delay(100);
    }
}

void stopSetupMode()
{
    bool hasWifi = (state.network.ssid.length() > 5); // Check of er echt iets in staat

    if (!hasWifi && state.network.is_setup_mode)
    {
        haltSystemWithInstruction();
    }
    else
    {
        // ... je normale 'terug naar klok' logica ...
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        lastStatus = -1;
        state.network.is_setup_mode = false;
        tft.fillScreen(TFT_BLACK);
        state.display.force_ticker_refresh = true;

        // LED feedback alleen hier!
        for (int i = 0; i < 3; i++)
        {
            digitalWrite(Config::pin_fingerprint_led, HIGH);
            delay(100);
            digitalWrite(Config::pin_fingerprint_led, LOW);
            delay(100);
        }
        Serial.println(F("[UI] Scherm hersteld na beheer-modus."));
    }
}
