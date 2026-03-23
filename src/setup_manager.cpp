/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Setup Manager: Alles wat te maken heeft met het opstarten van de ESP32, het initiëren van de hardware en het voorbereiden van de app voor gebruik.
 * Hierin staan functies zoals setup(), startAccessPoint(), en alle logica die nodig is om de app in de juiste staat te krijgen bij het opstarten.
 * Ook de logica voor het tekenen van het setup-scherm en het beheren van de overgangen tussen de verschillende setup-stappen wordt hier beheerd.
 * We hebben ervoor gekozen om deze functies in een aparte file te zetten om de code beter te organiseren en de setup-logica gescheiden te houden van de rest van de app, 
 * zodat we een duidelijk overzicht hebben van alles wat er gebeurt tijdens het opstarten en de setup-fase.
 */

#include "app_actions.h"

// Interne variabelen (alleen zichtbaar in dit bestand)
static unsigned long lastRedraw = 0;
const unsigned long REDRAW_INTERVAL = 1000; // 1 FPS is genoeg voor setup

// 1. STATUS BIJHOUDEN (De 'Grendel')
// We gebruiken -1 als startwaarde zodat hij de eerste keer altijd tekent
static int lastStatus = -1;

// Lokale hulpfunctie voor het WiFi icoon op de monitor (Zonder Sprite overhead!)
void App::drawMonitorWifi(int x, int y, int h)
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
void App::drawMonitorAlert(int x, int y, int size, uint16_t color = state.env.icon_base_color, bool unused = false)
{
    // Basis driehoek
    tft.fillTriangle(x + size / 2, y, x, y + size, x + size, y + size, color);
    // Uitroepteken in zwart
    int bw = size / 6;
    int bh = size / 2.2;
    tft.fillRoundRect(x + (size / 2) - (bw / 2), y + (size / 3), bw, bh, 1, TFT_BLACK);
    tft.fillCircle(x + (size / 2), y + size - (size / 6), size / 10, TFT_BLACK);
}

// --- PUBLIEKE FUNCTIES ---

void App::startAccessPoint()
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

    Serial.printf("Setup Mode gestart op 192.168.4.1\n");
}

void App::drawSetupModeActive()
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

    tft.fillCircle(200, 10, 4, state.display.touch_indicator_color);
}

void App::drawNetworkInfo()
{
    tft.fillScreen(TFT_BLACK);
    updateDisplayBrightness(Config::default_brightness);
    tft.drawRoundRect(2, 2, tft.width() - 4, tft.height() - 4, 10, TFT_GREEN);
    tft.drawBitmap(20, 20, image_DolphinSuccess_bits, 108, 57, TFT_WHITE);
    // tft.setTextFont(2);
    tft.setTextColor(TFT_GOLD);
    tft.drawCentreString("SYSTEEM START", 200, 56, 2);
    tft.setTextColor(TFT_SKYBLUE);
    tft.drawCentreString("mDNS: " + state.network.mdns, 160, 84, 4);
    tft.setTextColor(TFT_WHITE);
    tft.drawCentreString("IP: " + WiFi.localIP().toString(), 160, 120, 4);
    delay(2000); // Laat deze info 2 seconden zien voordat je verder gaat
}

// void App::showSetupInstructionPanel()  // momenteel niet gebruikt, de OWM lat lon worden in de setup geinjecteerd vanuit Preferences.h
// {
//     datSpr1.fillSprite(TFT_BLACK);
//     datSpr1.setTextColor(TFT_GOLD);
//     datSpr1.setTextDatum(MC_DATUM); // Midden-gecentreerd
//     datSpr1.drawString("WEER SETUP NODIG", datSpr1.width() / 2, 20, 2);
//     datSpr1.setTextColor(TFT_WHITE);
//     datSpr1.drawString("Ga naar:", datSpr1.width() / 2, 50, 2);
//     datSpr1.setTextColor(TFT_SKYBLUE);
//     datSpr1.drawString(WiFi.localIP().toString(), datSpr1.width() / 2, 75, 4);
//     datSpr1.setTextColor(TFT_WHITE);
//     datSpr1.drawString("en voer je API key in", datSpr1.width() / 2, 110, 2);
//     datSpr1.pushSprite(Config::data_x, Config::data_y);
//     delay(100); // Korte pauze om te voorkomen dat dit te snel hertekent
// }


void App::drawSystemMonitor()
{
    // 1. EENMALIGE BASIS (Het kader en de labels)
    if (!state.display.show_sm_bg_drawn)
    {
        tft.fillScreen(TFT_BLACK);
        tft.drawFastHLine(5, 25, 310, TFT_DARKGREY);
        tft.drawFastHLine(5, 140, 310, TFT_DARKGREY);

        tft.setTextFont(2);
        tft.setTextColor(TFT_LIGHTGREY);
        tft.drawString("Heap:", 15, 60);
        tft.drawString("MaxBlok:", 210, 60);
        tft.drawString("B-Light:", 15, 80);
        tft.drawString("OWM:", 210, 80);
        tft.drawString("C-Temp:", 15, 100);
        tft.drawString("Sensoren:", 15, 120);

        tft.setTextFont(1);
        tft.drawCentreString("HOLD 3s TO EXIT", 260, 150, 1);
        state.display.show_sm_bg_drawn = true;
    }

    // 2. DYNAMISCHE DATA (Met achtergrondkleur om flikkeren te voorkomen)
    updateDateTimeStrings();



    tft.setTextFont(2);

    // Bovenbalk: Tijd & Datum
    // Door TFT_BLACK als tweede parameter te geven, wist de tekst zichzelf

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char topRow[32];
    snprintf(topRow, sizeof(topRow), "%s  |  %s", state.env.current_time_str, state.env.current_date_str);
    tft.drawString(topRow, 10, 5);

    tft.fillCircle(200, 10, 4, state.display.touch_indicator_color); // Touch feedback indicator

    drawMonitorWifi(295, 5, 15);

    // Status melding
    int x = 270; int y = 30; int h = 20;
    if (state.env.health != HEALTH_OK)
    {
        drawMonitorAlert(x, y, h, state.env.get_current_alert_color(), true);
        tft.setTextColor(state.env.get_current_alert_color(), TFT_BLACK);
        tft.drawString("SYSTEEM ALERT!          ", 10, 35); // Spaties wissen oude tekst
    }
    else
    {
        drawMonitorAlert(x, y, h, state.env.icon_base_color, true);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("SYSTEEM STATUS: OPTIMAAL", 10, 35);
    }

    // Variabelen voor actuele metingen
    char heapBuffer[32];
    char maxAllocBuffer[32];
    char tempBuffer[32];
    char sensorBuffer[32];
    char pwmBuffer[32];
    char owmBuffer[20];
    
    // We voegen de actuele metingen per onderdeel samen in één string
    snprintf(heapBuffer, sizeof(heapBuffer), "%dkB (%dkB min)   ", ESP.getFreeHeap() / 1024, ESP.getMinFreeHeap() / 1024);
    snprintf(maxAllocBuffer, sizeof(maxAllocBuffer), "%dkB   ", ESP.getMaxAllocHeap() / 1024);
    snprintf(tempBuffer, sizeof(tempBuffer), "%.0fC (max %.0fC)  ", state.env.case_temp, Config::system_temp_warning);
    snprintf(sensorBuffer, sizeof(sensorBuffer), "AHT: %.1fC (%.1fC) BMP: %.1f hPa  ", state.env.temp_local, state.env.raw_temp_local, state.env.press_local);
    snprintf(pwmBuffer, sizeof(pwmBuffer), "PWM: %d%%   ", map(state.display.backlight_pwm, 0, 255, 0, 100));

    // --- OWM Status bepalen voor display ---
    uint16_t owmColor = TFT_WHITE;
    if (state.network.last_owm_http_code == 200) {
        snprintf(owmBuffer, sizeof(owmBuffer), "OK!   ");
        owmColor = TFT_GREEN;
    } else if (state.network.last_owm_http_code == 0) {
        snprintf(owmBuffer, sizeof(owmBuffer), "WAIT.. ");
        owmColor = TFT_YELLOW;
    } else {
        snprintf(owmBuffer, sizeof(owmBuffer), "E%d ", state.network.last_owm_http_code);
        owmColor = TFT_RED;
    }

    tft.setTextColor(TFT_WHITE, TFT_BLACK); // De zwarte achtergrond wast de oude tekst weg
    tft.setTextFont(2);
    tft.drawString(heapBuffer, 85, 60); // Teken de hele handel op een vaste positie

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(maxAllocBuffer, 270, 60); // Teken de hele handel op een vaste positie

    tft.setTextColor(state.env.health == HEALTH_OK ? TFT_WHITE : TFT_ORANGE, TFT_BLACK);
    tft.drawString(tempBuffer, 85, 100); // Teken de hele handel op een vaste positie
    tft.setTextColor(state.env.health == HEALTH_OK ? TFT_WHITE : TFT_ORANGE, TFT_BLACK);
    tft.drawString(pwmBuffer, 85, 80); // Teken de hele handel op een vaste positie
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
    tft.setTextColor(owmColor, TFT_BLACK);
    tft.drawString(owmBuffer, 270, 80); 

    // Uptime (onderaan)
    tft.setTextFont(1);
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    char uptimeStr[32];
    uint32_t sec = millis() / 1000;
    snprintf(uptimeStr, sizeof(uptimeStr), "UPTIME: %02du %02dm %02ds", (sec / 3600), (sec % 3600) / 60, sec % 60);
    tft.drawString(uptimeStr, 15, 150);
}

void App::haltSystemWithInstruction()
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

    Serial.printf("[SYSTEM] Systeem bevroren. Wacht op power-cycle...\n");

    // 3. De 'Freeze'
    // We laten de Buck aan (digitalWrite LOW), anders zie je de tekst niet.
    while (true)
    {
        yield(); // Voorkom dat de Watchdog de ESP herstart
        delay(100);
    }
}

void App::stopSetupMode()
{
    bool hasWifi = (state.network.ssid.length() > 5); // Check of er echt iets in staat

    if (!hasWifi && state.network.is_setup_mode)
    {
        App::haltSystemWithInstruction();
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
        Serial.printf("[UI] Scherm hersteld na beheer-modus.\n");
    }
}
