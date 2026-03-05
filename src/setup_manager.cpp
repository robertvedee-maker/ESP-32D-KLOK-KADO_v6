#include "global_data.h"
#include "config.h"
#include "helpers.h"
#include "bitmaps/BootImages.h"

#include "Setup_Manager.h"
#include <WiFi.h>
// #include <WebServer.h> // Of je Async variant
#include <ESPAsyncWebServer.h>
#include <TFT_eSPI.h>

extern void drawQRCodeOnTFT(const char *data, int x, int y, int scale);
extern void updateDateTimeStrings();

// Interne variabelen (alleen zichtbaar in dit bestand)
static unsigned long lastRedraw = 0;
const unsigned long REDRAW_INTERVAL = 1000; // 1 FPS is genoeg voor setup

static bool monitorBackgroundDrawn = false;

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

void startSetupMode()
{
    state.network.is_setup_mode = true;
    state.network.server_start_time = millis();

    // Start AP Mode
    WiFi.softAP("Kado-Klok-Setup", "");
    Serial.println(F("[SETUP] AP Gestart: Kado-Klok-Setup"));

    // Forceer eerste tekenbeurt
    lastRedraw = 0;
}

void runSetupCycle()
{
    // 1. Check op 10-minuten timeout
    if (millis() - state.network.server_start_time > 600000UL)
    {
        stopSetupMode();
        return;
    }

    // 2. Scherm verversen (Timer-gebaseerd tegen flikkeren)
    if (millis() - lastRedraw >= REDRAW_INTERVAL)
    {
        int stations = WiFi.softAPgetStationNum();

        if (stations == 0)
        {
            drawStep("STAP 1: VERBINDEN", "Scan WiFi-netwerk:", "SSID: Kado-Klok-Setup", TFT_GOLD);
            const char *qrWiFi = "WIFI:S:Kado-Klok-Setup;T:nopass;;";
            drawQRCodeOnTFT(qrWiFi, 195, 35, 3);
        }
        else
        {
            drawStep("STAP 2: CONFIGUREREN", "Verbonden! Ga naar:", "http://192.168.4.1", TFT_GREEN);
            const char *qrURL = "http://192.168.4.1";
            drawQRCodeOnTFT(qrURL, 195, 35, 3);
        }

        lastRedraw = millis();
    }
}

void drawSetupModeActive()
{
    // 1. Voorbereiding van het scherm
    // tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(2, 2, 316, 166, 10, TFT_GOLD);
    tft.drawBitmap(15, 15, image_DolphinWait_bits, 59, 54, TFT_WHITE);

    // Bepaal de status
    int stations = WiFi.softAPgetStationNum();
    unsigned long elapsed = millis() - state.network.server_start_time;
    unsigned long timeout = 600000UL; // 10 minuten
    char url[50];

    // --- SCENARIO A: HANDMATIG BEHEER (De 10-minuten modus) ---
    // We herkennen dit doordat de server actief is, maar we niet in de eerste AP-setup zitten
    if (state.network.web_server_active && WiFi.status() == WL_CONNECTED)
    {
        tft.setTextColor(TFT_GOLD);
        tft.setTextFont(2);
        tft.drawString("BEHEER MODUS ACTIEF", 15, 80);

        tft.setTextColor(TFT_WHITE);
        tft.drawString("Pas instellingen aan via:", 15, 100);
        tft.setTextColor(TFT_SKYBLUE);

        // tft.drawString("mDNS: " + state.network.mdns, 15, 120);

        tft.setTextColor(TFT_WHITE);
        snprintf(url, sizeof(url), "IP: %s", WiFi.localIP().toString().c_str());
        tft.drawString(url, 15, 120);

        // QR Code naar de lokale instellingen
        drawQRCodeOnTFT(url, 195, 35, 3);

        // Aftelbalk onderaan
        int barWidth = map(elapsed, 0, timeout, 310, 0);
        barWidth = constrain(barWidth, 0, 310);
        tft.drawRect(5, 160, 310, 5, TFT_DARKGREY);
        tft.fillRect(5, 160, barWidth, 5, TFT_GOLD);

        tft.setTextFont(1);
        tft.setTextColor(TFT_LIGHTGREY);
        tft.drawCentreString("Houd 3 sec. vast om te sluiten", 100, 145, 1);
    }
    // --- SCENARIO B: EERSTE SETUP (Stap 1 of 2) ---
    else
    {
        if (stations == 0) // STAP 1: Nog niet verbonden
        {
            tft.setTextColor(TFT_GOLD);
            tft.setTextFont(2);
            tft.drawString("STAP 1: VERBINDEN", 15, 80);
            tft.setTextColor(TFT_WHITE);
            tft.drawString("Scan WiFi-netwerk:", 15, 100);
            tft.setTextColor(TFT_SKYBLUE);
            tft.drawString("SSID: Kado-Klok-Setup", 15, 120);

            drawQRCodeOnTFT("WIFI:S:Kado-Klok-Setup;T:nopass;;", 195, 35, 3);
        }
        else // STAP 2: Verbonden met AP, wacht op config
        {
            tft.setTextColor(TFT_GREEN);
            tft.setTextFont(2);
            tft.drawString("STAP 2: CONFIGUREREN", 15, 80);
            tft.setTextColor(TFT_WHITE);
            tft.drawString("Verbonden! Scan nu om", 15, 100);
            tft.drawString("de klok in te stellen:", 15, 120);

            tft.setTextColor(TFT_SKYBLUE);
            snprintf(url, sizeof(url), "http://%s", WiFi.localIP().toString().c_str());
            tft.drawString(url, 15, 140);
            drawQRCodeOnTFT(url, 195, 35, 3);
        }
    }

    // Algemene footer
    tft.setTextFont(1);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.drawCentreString("SCAN MIJ", 245, 145, 1);
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
        tft.drawString("Heap:", 15, 70);
        tft.drawString("MaxBlok:", 15, 92);
        tft.drawString("Temp:", 15, 114);
        // tft.drawString("RSSI:", 170, 92);

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

    // Kolom 1 Data
    char heapBuffer[32];
    // We voegen de actuele heap en de minimum heap samen in één string
    snprintf(heapBuffer, sizeof(heapBuffer), "%dkB (%dkB min)   ", ESP.getFreeHeap() / 1024, ESP.getMinFreeHeap() / 1024);

    tft.setTextColor(TFT_WHITE, TFT_BLACK); // De zwarte achtergrond wast de oude tekst weg
    tft.setTextFont(2);
    tft.drawString(heapBuffer, 75, 70); // Teken de hele handel op een vaste positie

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawNumber(ESP.getMaxAllocHeap() / 1024, 75, 92);
    tft.drawString(" KB   ", 115, 92);

    char tempBuffer[32];
    // We voegen de temperatuur en de limiet samen
    snprintf(tempBuffer, sizeof(tempBuffer), "%.1fC (max %.0fC)  ", state.env.case_temp, Config::system_temp_threshold);

    tft.setTextColor(state.env.health == HEALTH_OK ? TFT_WHITE : TFT_ORANGE, TFT_BLACK);
    tft.drawString(tempBuffer, 75, 114); // Teken de hele handel op een vaste positie

    // tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // tft.drawNumber(WiFi.RSSI(), 220, 92);
    // tft.drawString(" dBm   ", 260, 92);

    // Uptime (onderaan)
    tft.setTextFont(1);
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    char uptimeStr[32];
    uint32_t sec = millis() / 1000;
    snprintf(uptimeStr, sizeof(uptimeStr), "UPTIME: %02du %02dm %02ds", (sec / 3600), (sec % 3600) / 60, sec % 60);
    tft.drawString(uptimeStr, 15, 150);
}

void stopSetupMode()
{
    WiFi.softAPdisconnect(true);
    state.network.is_setup_mode = false;
    state.network.web_server_active = false;

    tft.fillScreen(TFT_BLACK);
    state.display.force_ticker_refresh = true;
    Serial.println(F("[SETUP] Modus afgesloten."));
}
