/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Helper functies en display setup (zoals het tekenen van de panelen, het beheren van de backlight, etc.)
 * 
 * Deze file bevat functies die worden gebruikt door meerdere onderdelen van de app, 
 * zoals het tekenen van de klok, het tekenen van de data panelen, het tekenen van de ticker, 
 * en het beheren van de backlight.
 * 
 * We hebben ervoor gekozen om deze functies in een aparte file te zetten om de code beter te organiseren en de andere files overzichtelijk te houden.
 * 
 * In deze file staan ook de functies voor het opzetten van de WiFi-verbinding en het beheren van de eco-mode, 
 * omdat die nauw verbonden zijn met de netwerkstatus die we in de centrale 'state' struct bijhouden.
 * 
 * Op deze manier kunnen we de netwerk-gerelateerde functies gescheiden houden van de display-logica, 
 * terwijl we toch een centrale plek hebben voor alle 'helper' functies die door de app worden gebruikt.
 */

#include "helpers.h"
#include "config.h" // Toegevoegd voor Config:: pinnen en waarden
#include "display_logic.h"
#include "driver/ledc.h" // Voor PWM controle van de backlight
#include "env_sensors.h"
#include "global_data.h"
#include "leeftijd_calc.h"
#include "secret.h"
#include "bitmaps/weatherIcons40.h"
#include "bitmaps/weatherIcons68.h"
#include "weather_logic.h"
#include <WiFi.h>
#include "network_logic.h"
#include "web_config.h"

// 1. Hardware instanties (MOETEN hier gedefinieerd worden)
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite clkSpr = TFT_eSprite(&tft);
TFT_eSprite datSpr = TFT_eSprite(&tft);
TFT_eSprite datSpr2 = TFT_eSprite(&tft);
TFT_eSprite tckSpr = TFT_eSprite(&tft);

extern SystemState state; // Voor toegang tot de centrale 'state' struct

// 3. Helper functies (De "Vakmannen")
String getWindRoos(int graden)
{
    const char *roos[] = {"N", "NNO", "NO", "ONO", "O", "OZO", "ZO", "ZZO", "Z", "ZZW", "ZW", "WZW", "W", "WNW", "NW", "NNW"};
    int index = (int)((graden + 11.25) / 22.5) % 16;
    return String(roos[index]);
}

int getBeaufort(float ms)
{
    if (ms < 0.3)
        return 0;
    if (ms < 1.6)
        return 1;
    if (ms < 3.4)
        return 2;
    if (ms < 5.5)
        return 3;
    if (ms < 8.0)
        return 4;
    if (ms < 10.8)
        return 5;
    if (ms < 13.9)
        return 6;
    if (ms < 17.2)
        return 7;
    if (ms < 20.8)
        return 8;
    if (ms < 24.5)
        return 9;
    if (ms < 28.5)
        return 10;
    if (ms < 32.7)
        return 11;
    return 12;
}

int getBeaufort(float ms);
String getWindRoos(int graden);
void updateDataPaneelVandaag();
void updateDataPaneelForecast();
void updateTickerSegments();
void setupWiFi();
void handleWiFiEco();

// Voeg dit toe onderaan je helpers.cpp als tijdelijke fix:
uint16_t getIconColor(int conditionCode)
{
    // Simpele mapping van OpenWeatherMap codes naar kleuren
    if (conditionCode >= 200 && conditionCode < 300)
        return TFT_SILVER; // Onweer
    if (conditionCode >= 300 && conditionCode < 600)
        return TFT_BLUE; // Regen
    if (conditionCode >= 600 && conditionCode < 700)
        return TFT_WHITE; // Sneeuw
    if (conditionCode == 800)
        return TFT_YELLOW; // Helder
    return TFT_LIGHTGREY;  // Bewolkt/Rest
}

// --- INITIALISATIE ---

//          --- DISPLAY SETUP FUNCTIE ---

void setupDisplay()
{
    // 1. Hardware onderdrukking (Dark Boot)
    pinMode(Config::pin_backlight, OUTPUT);
    digitalWrite(Config::pin_backlight, LOW);

    // 2. Initialiseer TFT
    tft.init();
#ifdef TFT_ROTATION
    tft.setRotation(TFT_ROTATION);
#else
    tft.setRotation(1);
#endif

    tft.fillScreen(TFT_BLACK);

    // 3. Haal de actuele schermmaten op (na rotatie!)
    int sw = tft.width();  // Geeft 320 op je huidige scherm
    int sh = tft.height(); // Geeft 240 op je huidige scherm

    // 4. Initialiseer de sprites volgens S.P.O.T.
    // Klok: Vaste afmetingen uit Config (150x150)
    clkSpr.createSprite(Config::clock_w, Config::clock_h);

    // Data Panelen: Dynamische breedte (320 - 150 = 170) x Vaste hoogte (150)
    int dataW = Config::get_data_width(sw);
    datSpr.createSprite(dataW, Config::data_h);
    datSpr2.createSprite(dataW, Config::data_h);

    // Ticker: Volledige schermbreedte x Hoogte uit Config (20)
    tckSpr.createSprite(sw, Config::ticker_h);

    // Serial.printf("[DISPLAY] Sprites aangemaakt: Klok(%dx%d), Data(%dx%d), Ticker(%dx%d)\n",
    //               Config::clock_w, Config::clock_h, dataW, Config::data_h, sw, Config::ticker_h);

    // 5. Initialiseer PWM & Fade-in naar start-helderheid
    ledcAttach(Config::pin_backlight, Config::pwm_freq, Config::pwm_res);
    updateDisplayBrightness(state.display.backlight_pwm);
}

//          --- BACKLIGHT FUNCTIES ---
void updateDisplayBrightness(int level)
{
    // Gebruik een 'static' variabele om de ÉCHTE hardware stand te onthouden
    // We zetten deze op -1 zodat hij bij de eerste start NOOIT gelijk is aan 'level'
    static int actual_hardware_level = -1;

    level = constrain(level, 0, 255);
    int delta = abs(level - actual_hardware_level);

    // Initialisatie check (Gebeurt slechts één keer)
    static bool hardware_initialized = false;
    if (!hardware_initialized)
    {
        // Serial.printf("[PWM] Hardware initialisatie naar: %d\n", level);
        ledcAttach(Config::pin_backlight, Config::pwm_freq, Config::pwm_res);
        ledcWrite(Config::pin_backlight, level);

        actual_hardware_level = level;       // Synchroniseer hardware status
        state.display.backlight_pwm = level; // Synchroniseer centrale state
        hardware_initialized = true;
        return;
    }

    // Als de hardware al op dit niveau staat, doen we niets
    if (delta == 0)
        return;

    // Serial.printf("[PWM] Update: %d (Was: %d, Delta: %d)\n", level, actual_hardware_level, delta);

    // SITUATIE 1: Grote sprong -> Fade
    if (delta > 10)
    {
        int current = actual_hardware_level; // Start faden vanaf waar de hardware NU staat
        int step = (level > current) ? 1 : -1;
        int step_delay = Config::fade_duration / delta;
        if (step_delay < 1)
            step_delay = 1;

        while (current != level)
        {
            current += step;
            ledcWrite(Config::pin_backlight, current);
            delay(step_delay);
        }
    }
    // SITUATIE 2: Kleine stap -> Direct
    else
    {
        ledcWrite(Config::pin_backlight, level);
    }

    // 4. Update de administratie (Zowel hardware status als centrale state)
    actual_hardware_level = level;
    state.display.backlight_pwm = level;
}

//          --- TICKER SEGMENT FUNCTIES ---
void addTickerSegment(String txt, uint16_t col)
{
    int w = tckSpr.textWidth(txt); // Meet direct hoe breed dit font is
    state.ticker_segments.push_back({txt, col, w});
    state.display.total_ticker_width += w; // Tel het op bij het totaal
}

//          --- KLOK UPDATE FUNCTIE ---
void updateClock()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        // Teken de wijzers (deze functie komt uit classic_clock.cpp)
        // We geven de uren, minuten en seconden door
        renderFace(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        // Stuur de complete klok-sprite naar het scherm (Links, boven)
        // clkSpr.pushSprite(0, 0);
    }
}

//          --- DATA PANEL UPDATE FUNCTIES ---
void updateDataPaneelVandaag()
{
    uint16_t TFT_BGD = state.env.is_night_mode ? Config::bg_color_night : Config::bg_color_day;
    uint16_t TFT_GFX = state.env.is_night_mode ? Config::text_color_night : Config::gfx_color_day;

    datSpr.fillSprite(TFT_BLACK);

    // --- 1. ICOON ---
    drawWeatherIcon(datSpr, 10, 5, 80, state.weather.current_icon, !state.env.is_night_mode);

    // --- 2. TEMPERATUUR ---
    datSpr.setTextFont(4);
    datSpr.setTextColor(TFT_WHITE, TFT_BLACK);
    datSpr.setTextDatum(TR_DATUM);
    int xPos = 88, yPos = 78;
    datSpr.drawString(String(state.weather.temp, 1), xPos - 20, yPos);
    datSpr.drawString("c", xPos, yPos);
    datSpr.drawCircle(xPos - 15, yPos + 2, 3, TFT_WHITE);
    datSpr.drawCircle(xPos - 15, yPos + 2, 4, TFT_WHITE);

    // --- 3. WINDKOMPAS ---
    int kX = 135, kY = 45, kR = 28;
    int graden = state.weather.wind_deg;
    datSpr.drawCircle(kX, kY, kR, TFT_BGD);
    datSpr.setTextFont(1);
    datSpr.setTextColor(TFT_GFX);
    datSpr.setTextDatum(MC_DATUM);
    datSpr.drawString("N", kX, kY - kR + 6);
    datSpr.drawString("O", kX + kR - 6, kY);
    datSpr.drawString("Z", kX, kY + kR - 6);
    datSpr.drawString("W", kX - kR + 6, kY);

    float rad = (graden - 90) * 0.0174533;
    float radBack = (graden + 90) * 0.0174533;
    float radSide1 = (graden) * 0.0174533;
    float radSide2 = (graden + 180) * 0.0174533;

    int tipX = kX + kR * cos(rad), tipY = kY + kR * sin(rad);
    int tailX = kX + kR * cos(radBack), tailY = kY + kR * sin(radBack);
    int s1x = kX + 5 * cos(radSide1), s1y = kY + 5 * sin(radSide1);
    int s2x = kX + 5 * cos(radSide2), s2y = kY + 5 * sin(radSide2);

    datSpr.fillTriangle(tipX, tipY, s1x, s1y, kX, kY, TFT_GOLD);
    datSpr.drawTriangle(tipX, tipY, s2x, s2y, kX, kY, TFT_GOLD);
    datSpr.fillTriangle(tailX, tailY, s1x, s1y, kX, kY, TFT_DARKGREY);
    datSpr.drawTriangle(tailX, tailY, s2x, s2y, kX, kY, TFT_DARKGREY);

    datSpr.setTextFont(2);
    datSpr.setTextColor(TFT_SKYBLUE);
    int bft = getBeaufort(state.weather.wind_speed);
    datSpr.drawString(String(bft) + " Bft", kX, kY + kR + 18);

    // --- 4. EXTRA INFO ---
    datSpr.setTextFont(2);
    datSpr.setTextDatum(BL_DATUM);
    datSpr.setTextColor(TFT_LIGHTGREY);
    datSpr.drawString("V: " + String((int)state.weather.humidity) + "% | UV: " + String(state.weather.uvi, 1), 5, 145);

    drawWifiIndicator(146, 130, 13);
    drawISOAlert(120, 125, 18, 0, state.env.is_alert_active);


    // --- 5. DE RELATIEVE HORIZON ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    int tlX_center = 85, tlY = 118;
    float pixelsPerUur = 15.0; // Eventueel SECRET_HORIZON_PIXELS_PER_HOUR uit config

    float nuDecimal = timeinfo.tm_hour + (timeinfo.tm_min / 60.0);
    float distOp = (float)state.env.sunrise_local - nuDecimal;
    float distOnder = (float)state.env.sunset_local - nuDecimal;

    datSpr.drawFastHLine(5, tlY + 1, 170, state.env.is_night_mode ? 0x2104 : TFT_DARKGREY);

    int xOp = tlX_center + (int)(distOp * pixelsPerUur);
    int xOnder = tlX_center + (int)(distOnder * pixelsPerUur);

    if (xOp > 0 && xOp < 170) {
        datSpr.fillCircle(xOp, tlY + 1, 4, TFT_YELLOW);
        datSpr.setTextDatum(BC_DATUM);
        datSpr.setTextFont(1);
        datSpr.drawString(String(state.env.sunrise_str), xOp, tlY - 5);
    }
    if (xOnder > 0 && xOnder < 170) {
        datSpr.fillCircle(xOnder, tlY + 1, 4, TFT_GOLD);
        datSpr.setTextDatum(BC_DATUM);
        datSpr.setTextFont(1);
        datSpr.drawString(String(state.env.sunset_str), xOnder, tlY - 5);
    }

    float moonPos = (nuDecimal < 12) ? nuDecimal + 12 : nuDecimal - 12;
    float distMaan = moonPos - nuDecimal;
    int xMaan = tlX_center + (int)(distMaan * pixelsPerUur);

    if (xMaan > 0 && xMaan < 170)
        drawMoonPhase(xMaan, tlY - 10, 5);
    datSpr.fillTriangle(tlX_center, tlY - 2, tlX_center - 4, tlY - 8, tlX_center + 4, tlY - 8, TFT_WHITE);
}

void updateDataPaneelForecast()
{
    datSpr2.fillSprite(TFT_BLACK); // We gebruiken datSpr voor beide panelen
    datSpr2.setTextColor(TFT_WHITE, TFT_BLACK);

    for (int i = 0; i < 3; i++) {
        int yPos = i * 50;
        auto& f = state.weather.forecast[i];

        time_t rawtime = (time_t)f.dt;
        struct tm* ti = localtime(&rawtime);

        char datumBuf[12];
        strftime(datumBuf, sizeof(datumBuf), "%d-%m", ti); // "28-01"

        datSpr2.setTextFont(2);
        datSpr2.setTextDatum(TR_DATUM);
        datSpr2.setTextColor(TFT_WHITE, TFT_BLACK);
        datSpr2.drawString(dagNamen[ti->tm_wday], 35, yPos + 10);

        datSpr2.setTextFont(1);
        datSpr2.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        datSpr2.drawString(datumBuf, 36, yPos + 30);

        datSpr2.setTextDatum(TR_DATUM);
        datSpr2.setTextFont(2);
        datSpr2.setTextColor(TFT_GOLD, TFT_BLACK);
        String tMinMax = String(f.temp_min, 1) + " - " + String(f.temp_max, 1);
        datSpr2.drawString(tMinMax, 165 - 12, yPos + 10);
        datSpr2.drawString("c", 165, yPos + 10);
        datSpr2.drawCircle(165 - 8, yPos + 11, 2, TFT_GOLD);

        datSpr2.setTextFont(1);
        datSpr2.setTextColor(TFT_SKYBLUE, TFT_BLACK);
        datSpr2.drawString(f.description, 165, yPos + 30);

        drawWeatherIcon(datSpr2, 40, yPos + 5, 40, f.icon_id, true);
        // const unsigned char* bitmap = getIcon40(f.icon_id, !state.env.is_night_mode);
        // datSpr.drawBitmap(65, yPos + 5, bitmap, 40, 40, TFT_WHITE);

        if (i < 2)
            datSpr2.drawFastHLine(0, yPos + 49, 170, 0x2104);
    }
}

//          --- NETWERK INFO PANELS ---
void showSetupInstructionPanel() {
    datSpr.fillSprite(TFT_BLACK);
    datSpr.setTextColor(TFT_GOLD);
    datSpr.setTextDatum(MC_DATUM); // Midden-gecentreerd
    
    datSpr.drawString("WEER SETUP NODIG", datSpr.width()/2, 20, 2);
    
    datSpr.setTextColor(TFT_WHITE);
    datSpr.drawString("Ga naar:", datSpr.width()/2, 50, 2);
    datSpr.setTextColor(TFT_SKYBLUE);
    datSpr.drawString(WiFi.localIP().toString(), datSpr.width()/2, 75, 4);
    
    datSpr.setTextColor(TFT_WHITE);
    datSpr.drawString("en voer je API key in", datSpr.width()/2, 110, 2);
    
    datSpr.pushSprite(Config::data_x, Config::data_y);
    delay(100); // Korte pauze om te voorkomen dat dit te snel hertekent
}

// --- 1. HET DOFIJN-SCHERM (Netwerk info) ---
void toonNetwerkInfo()
{
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(2, 2, tft.width() - 4, tft.height() - 4, 10, TFT_GREEN);
    tft.drawBitmap(20, 20, image_DolphinSuccess_bits, 108, 57, TFT_WHITE);

    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("SYSTEEM START", 140, 30);
    tft.setTextColor(TFT_SKYBLUE);
    tft.drawString("mDNS: " + state.network.mdns, 140, 60);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("IP: " + WiFi.localIP().toString(), 140, 80);

    delay(3000);
    // tft.fillScreen(TFT_BLACK);
}

void drawSetupModeActive(){
    // Opstart scherm voor Setup Mode
    tft.fillScreen(TFT_BLACK);
    tft.drawRoundRect(2, 2, tft.width() - 4, tft.height() - 4, 10, TFT_GOLD);
    tft.drawBitmap(20, 20, image_DolphinWait_bits, 59, 54, TFT_WHITE);

    tft.setTextFont(2);
    tft.setTextColor(TFT_GOLD);
    tft.drawString("SETUP MODUS ACTIEF", 140, 30);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Verbind met:", 140, 60);
    tft.setTextColor(TFT_SKYBLUE);
    tft.drawString("Kado-Klok-Setup", 140, 80);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Ga naar: 192.168.4.1", 140, 115);
}

// ---   TEKENFUNCTIES ---
void drawMoonPhase(int x, int y, int radius)
{
    static float filteredPhase = 0.5;
    static long lastMoonCheck = 0;

    // De maan beweegt traag, dus 1x per uur updaten is ruim voldoende
    if (millis() - lastMoonCheck > 3600000 || lastMoonCheck == 0)
    {
        // Haal de phase op uit de forecast van vandaag (index 0)
        filteredPhase = state.weather.forecast[0].moon_phase;
        lastMoonCheck = millis();
    }

    datSpr.setTextFont(1);
    datSpr.setTextColor(TFT_WHITE);
    datSpr.setTextDatum(TL_DATUM);

    // De zon-tijden komen nu uit state.env (onze char-arrays)
    datSpr.drawString(state.env.sunrise_str, 24, y - 3);

    datSpr.setTextDatum(TR_DATUM);
    datSpr.drawString(state.env.sunset_str, x - 14, y - 3);

    // Teken de basis van de maan
    datSpr.fillCircle(10, y, radius, 0xFFDA); // Warm goud/wit
    datSpr.fillCircle(x, y, radius, 0xFFDA);

    // De schaduw-logica (vereenvoudigd voor stabiliteit)
    float offset;
    if (filteredPhase <= 0.5)
    {
        // Wassende maan
        offset = map(filteredPhase * 1000, 0, 500, radius * 2, 0);
        datSpr.fillCircle(x - offset, y, radius + 1, TFT_BLACK);
    }
    else
    {
        // Afnemende maan
        offset = map(filteredPhase * 1000, 500, 1000, 0, radius * 2);
        datSpr.fillCircle(x + offset, y, radius + 1, TFT_BLACK);
    }
}

void drawWifiIndicator(int x, int y, int h)
{
    static float filteredRSSI = -70.0;
    static long lastWifiCheck = 0;
    const float alpha = 0.05;

    // 1. Signaalmeting
    if (millis() - lastWifiCheck > 5000 || lastWifiCheck == 0)
    {
        int rawRSSI = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -110;
        filteredRSSI = (alpha * rawRSSI) + (1.0 - alpha) * filteredRSSI;
        lastWifiCheck = millis();
    }

    // 2. Bepaal de kleuren
    uint16_t ecoGreen = 0x05E0;     // Fris Eco-groen
    uint16_t standbyGreen = 0x02C0; // Gedimd groen voor stand-by

    if (WiFi.status() == WL_CONNECTED)
    {
        // --- STATUS: VERBONDEN ---
        int bars = 0;
        if (filteredRSSI > -55)
            bars = 4;
        else if (filteredRSSI > -70)
            bars = 3;
        else if (filteredRSSI > -85)
            bars = 2;
        else if (filteredRSSI > -98)
            bars = 1;
        else
            bars = 0;

        // Als we verbonden zijn door een automatische update-cycle (On-Demand)
        // kunnen we de bars ook dat groene kleurtje geven ipv de standaard blauw/groen
        bool isUpdating = (state.network.wifi_mode == 1);

        for (int i = 0; i < 4; i++)
        {
            uint16_t activeColor;
            if (isUpdating)
            {
                activeColor = ecoGreen; // Alles in Eco-groen tijdens de update
            }
            else
            {
                activeColor = (bars > 1) ? 0x7BEF : (bars == 1 ? 0xFDA0 : 0xF800);
            }

            uint16_t color = (i < bars) ? activeColor : state.env.icon_base_color;
            datSpr.fillRoundRect(x + (i * 6), y + (h - (i * 4) - 4), 4, (i * 4) + 4, 1, color);
        }
    }
    else
    {
        // --- STATUS: UIT ---
        uint16_t statusColor;

        if (state.network.wifi_mode == 1)
        {
            // Mode: On-Demand (Wacht op update slot)
            statusColor = standbyGreen;
        }
        else
        {
            // Mode: Eco-Nacht of uit
            statusColor = state.env.icon_base_color;
        }

        for (int i = 0; i < 4; i++)
        {
            datSpr.fillRoundRect(x + (i * 6), y + (h - (i * 4) - 4), 4, (i * 4) + 4, 1, statusColor);
        }
    }
}

void drawISOAlert(int x, int y, int size, uint16_t color, bool active)
{
    // Bepaal de kleur:
    // Als 'color' niet de default TFT_BLACK (0) is, gebruik die.
    // Anders: gebruik de logica uit onze state.
    uint16_t signColor = (color != 0) ? color : state.env.get_current_alert_color();

    // Gebruik de actieve status uit de state als 'active' niet expliciet meegegeven is
    bool isActuallyActive = active || state.env.is_alert_active;

    // 1. De basis driehoek (altijd zwart of achtergrond)
    datSpr.fillTriangle(x + size / 2, y, x, y + size, x + size, y + size, TFT_BLACK);

    // 2. Het gekleurde binnenvlak (Dit wordt nu 0x2104 of TFT_GREENYELLOW)
    int offset = size / 10;
    if (offset < 2)
        offset = 2;
    datSpr.fillTriangle(x + size / 2, y + (offset * 1.5), x + offset, y + size - (offset / 2), x + size - offset, y + size - (offset / 2), signColor);

    // 3. Teken het '!' teken alleen als er echt een alarm is
    if (isActuallyActive)
    {
        int barWidth = size / 6;
        int barHeight = size / 2.2;
        datSpr.fillRoundRect(x + (size / 2) - (barWidth / 2), y + (size / 3), barWidth, barHeight, 1, TFT_BLACK);
        datSpr.fillCircle(x + (size / 2), y + size - (size / 4), size / 10, TFT_BLACK);
    }
}

void drawWeatherIcon(TFT_eSprite &spr, int x, int y, int size, int iconId, bool isDay)
{
    const unsigned char *bitmap;
    uint16_t kleur = getIconColor(iconId);

    if (size == 80)
    {
        bitmap = getIcon68(iconId, isDay);
        spr.drawBitmap(x, y, bitmap, 68, 68, kleur, TFT_BLACK);
    }
    else
    {
        bitmap = getIcon40(iconId, isDay);
        spr.drawBitmap(x, y, bitmap, 40, 40, kleur, TFT_BLACK);
    }
}

// ---  HARDWARE INTERACTIE FUNCTIES ---
void handleHardware()
{
    // 1. TOUCH SENSOR (GPIO 13)
    int touchVal = touchRead(13);
    static bool isTouching = false;
    static unsigned long touchStart = 0;

    // Kalibratie tip: touchVal onder de 25 is meestal een 'hit'
    if (touchVal < 25)
    {
        if (!isTouching)
        {
            touchStart = millis();
            isTouching = true;
        }
    }
    else if (isTouching)
    {
        unsigned long duration = millis() - touchStart;
        // Korte tik: Mute toggle
        if (duration > 50 && duration < 800)
        {
            state.env.alert_muted = !state.env.alert_muted;
        }
        isTouching = false;
    }

    // 2. LED LOGICA (GPIO 2)
    if (state.env.is_alert_active)
    {
        if (state.env.alert_muted)
        {
            // Muted: Subtiel 'hartslag' effect (knippert heel kort elke 3 seconden)
            digitalWrite(2, (millis() % 3000 < 50));
        }
        else
        {
            // Actief: Opvallend knipperen (Safety Car tempo)
            digitalWrite(2, (millis() % 1000 < 500));
        }
    }
    else
    {
        digitalWrite(2, LOW); // Ruststand
    }
}

void handleTouchLadder()
{
    int touchVal = touchRead(13);
    static unsigned long touchStart = 0;
    static bool isTouching = false;
    static bool stage1Feedback = false;
    static bool stage2Triggered = false;

    if (touchVal < 55)
    {   // AANRAKING DETECTEERD
        // Teken een klein oranje/geel cirkeltje rechtsboven de analoge klok om visuele feedback te geven dat we een aanraking detecteren (en dat de ladder actief is)
        tft.fillCircle(145, 7, 4, TFT_GOLD);
        // Serial.printf("[TOUCH] Aanraking gedetecteerd (waarde: %d)\n", touchVal);

        if (!isTouching)
        {
            touchStart = millis();
            isTouching = true;
            state.env.is_touching = true; // Update de centrale state voor de touch status
            stage1Feedback = false;
            stage2Triggered = false;
        }

        unsigned long duration = millis() - touchStart;

        // FEEDBACK FASE 1 (0-2 sec): Rustig knipperen 
        if (duration < 2000)
        {
            digitalWrite(2, (millis() % 400 < 200));
        }
        // FEEDBACK FASE 2 (2-5 sec): Sneller knipperen (Mute zone)
        else if (duration >= 2000 && duration < 5000)
        {
            digitalWrite(2, (millis() % 200 < 100));
            stage1Feedback = true; // We weten nu dat we in de 'Mute' zone zitten
        }
        // ACTIE FASE 3 (5+ sec): WiFi Config (Directe actie)
        else if (duration >= 5000 && !stage2Triggered)
        {
            // Bevestig met 3 flitsen
            for (int i = 0; i < 3; i++)
            {
                digitalWrite(2, HIGH);
                delay(50);
                digitalWrite(2, LOW);
                delay(50);
            }

            // Start de WiFi en Server
            activateWiFiAndServer();
            stage2Triggered = true;
        }
    }
    else
    { // LOSGELATEN

        // Wis het cirkeltje als er geen aanraking is (teken een zwart rondje eroverheen)
        if (isTouching)
            tft.fillCircle(145, 7, 4, TFT_BLACK);

        if (isTouching)
        {
            unsigned long eindDuur = millis() - touchStart;

            // Alleen de Mute actie uitvoeren als we LOSLATEN in de 2-5 sec zone
            if (eindDuur >= 2000 && eindDuur < 5000 && !stage2Triggered)
            {
                state.env.alert_muted = !state.env.alert_muted;
                state.display.ticker_x = -state.display.total_ticker_width; // Forceer wissel
                Serial.println(F("[TOUCH] Alert Mute Toggle uitgevoerd"));
            }

            // LED status herstellen (Aan als server draait, anders uit)
            digitalWrite(2, state.network.web_server_active ? HIGH : LOW);

            isTouching = false;
            stage1Feedback = false;
            stage2Triggered = false;
            touchStart = 0;
            state.env.is_touching = false; // Update de centrale state voor de touch status
        }   
    }
}

// even kijken of we dit nog nodig hebben of gebruiken om de status van handleTouchLadder te ondersteunen
void manageStatusLed()
{
    if (state.env.is_touching) return;
    
    if (state.network.web_server_active)
    {
        // De server is actief: LED aan, maar elke 5 sec (5000ms) een flits van 100ms uit
        bool ledStatus = (millis() % 5000 > 100);
        digitalWrite(2, ledStatus);
    }
    else if (state.env.is_alert_active && !state.env.alert_muted)
    {
        // Je bestaande alarm-knipper logica
        digitalWrite(2, (millis() % 1000 < 500));
    }
    else
    {
        digitalWrite(2, LOW);
    }
}

void manageAlertTimeout()
{
    if (!state.env.alert_muted && millis() > Config::alert_timeout)
    {
        state.env.alert_muted = true;         // Terug naar rustige stand
        state.display.ticker_x = tft.width(); // Reset ticker weer
        updateTickerSegments();
        Serial.println(F("[ALARM] Auto-timeout: Ticker terug naar normaal."));
    }
}

