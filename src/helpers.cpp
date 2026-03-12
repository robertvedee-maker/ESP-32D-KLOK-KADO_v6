/*
 * (c)2026 R van Dorland - Licensed under MIT License
 */

#include "helpers.h"
#include "config.h" // Toegevoegd voor Config:: pinnen en waarden
#include "display_logic.h"
#include "driver/ledc.h" // Voor PWM controle van de backlight
#include "env_sensors.h"
#include "global_data.h"
#include "daynight.h"
#include "leeftijd_calc.h"
#include "secret.h"
#include "bitmaps/weatherIcons40.h"
#include "bitmaps/weatherIcons68.h"
#include "weather_logic.h"
#include <WiFi.h>
#include "network_logic.h"
#include "web_config.h"
#include <qrcode.h>

// --- HULPFUNCTIES: Kleine functies die in meerdere files worden gebruikt, zoals het omzetten van data naar tekst of symbolen, of het tekenen van veelgebruikte elementen op het scherm. ---
void addTickerSegment(String txt, uint16_t col);
void drawQRCodeOnTFT(const char *data, int x, int y, int scale);
void drawWeatherIcon(TFT_eSprite &spr, int x, int y, int size, int iconId, bool isDay);
void drawISOAlert(int x, int y, int size, uint16_t color, bool active);
void drawWifiIndicator(int x, int y, int h);
void evaluateSystemSafety();
void finalizeSetupAndShowDashboard();
void finalizeUIAfterSetup();
void handleHardware();
void handleTouchLadder();
void manageDataPanels();
void manageStatusLed();
void manageTimeFunctions();
void powerDownWiFi();
void setupDisplay();
void updateClock();
void updateDataPaneelVandaag();
void updateDataPaneelForecast();
void updateDisplayBrightness(int level);
void updateTickerSegments();
void updateTouchLedFeedback(unsigned long duration);
void vulEasterEggTekst(char *buffer, size_t bufferSize, int gDag, int gMaand, int gJaar, int forceVariant);
bool vulGepersonaliseerdFeitje(char *buffer, size_t bufferSize);

extern SystemState state; // Voor toegang tot de centrale 'state' struct

// --- LOGICA VOOR HET OMZETTEN VAN DATA NAAR TEKST OF SYMBOLEN ---

// Simpele mapping van OpenWeatherMap codes naar kleuren
uint16_t getIconColor(int conditionCode)
{
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

// Windroos en Beaufort schaal
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

int berekenMinutenTotUpdate()
{
    // 1. Haal de huidige tijd (Epoch) op
    uint32_t nu = time(nullptr); // Haal de huidige Unix timestamp op

    // 2. Bepaal wanneer de volgende update zou moeten zijn (last + 30 min)
    uint32_t volgendeUpdate = state.weather.last_update_epoch + (30 * 60);

    // 3. Bereken het verschil
    if (nu >= volgendeUpdate)
        return 0; // Tijd is al om!

    return (int)((volgendeUpdate - nu) / 60);
}

// Barometer tekst op basis van druk en trend, met tijdsafhankelijke interpretatie
const char *getBaroText(char *buffer, size_t bufferSize)
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        snprintf(buffer, bufferSize, "TIJD FOUT");
        return buffer;
    }

    int h = timeinfo.tm_hour;
    float p = state.env.pressure;
    int t = state.env.baro_trend;

    // 1. BASISWAARDE (Altijd een vangnet)
    const char *result = "ZONNIG & STABIEL";

    // 2. EXTREMEN (Gaan voor alles)
    if (p < 980)
        result = "STORM";
    else if (p > 1035)
        result = "ZEER DROOG";

    // 3. DAGDELEN (Nacht & Avond)
    else if (h >= 23 || h < 6)
    {
        if (t < 0)
            result = "REGEN MORGEN?";
        else if (p > 1018)
            result = "MOOIE MORGEN";
        else
            result = "RUSTIGE NACHT";
    }
    else if (h >= 17)
    {
        if (t < 0)
            result = "BETREKT VANAVOND";
        else if (p > 1018)
            result = "HELDERE AVOND";
        else
            result = "KOELE AVOND";
    }

    // 4. STANDAARD DAG (06:00 - 17:00)
    else
    {
        // EERST: Check of OWM actuele info heeft (JOUW VERFIJNING)
        const char *owmText = state.weather.description.c_str();
        if (strlen(owmText) > 0)
        {
            String desc = String(owmText);
            desc.trim();
            if (desc.length() >= 17)
            {
                int firstSpace = desc.indexOf(' ');
                if (firstSpace != -1)
                {
                    desc = desc.substring(firstSpace + 1);
                    desc.trim();
                }
            }
            if (desc.length() > 0)
            {
                desc.toUpperCase();
                snprintf(buffer, bufferSize, "%s", desc.substring(0, 16).c_str());
                return buffer; // We geven de OWM tekst direct terug
            }
        }

        // TWEEDE: Fallback op Barometer als OWM leeg is
        if (p < 1000)
            result = (t < 0) ? "REGEN/WIND" : "WISSELVALLIG";
        else if (p < 1012)
            result = (t > 0) ? "VERBETERING" : "BEWOLKT";
        else if (p < 1022)
            result = (t < 0) ? "LICHT BEWOLKT" : "MOOI WEER";
    }

    // Alles wat niet via OWM is gegaan, komt hier in de buffer
    snprintf(buffer, bufferSize, "%s", result);
    return buffer;
}

// --- INITIALISATIE ---

// --- DISPLAY SETUP FUNCTIE ---
void setupDisplay()
{
    // 2. Initialiseer TFT
    tft.init();
#ifdef TFT_ROTATION
    tft.setRotation(TFT_ROTATION);
#else
    tft.setRotation(1);
#endif

    // 3. Zet de Buck nu AAN
    digitalWrite(Config::pin_buck_enable, LOW);

    // 4. ONMIDDELLIJK zwart schrijven (vóór de controller zijn eigen witte beeld vult)
    tft.fillScreen(TFT_BLACK);

    // 3. Haal de actuele schermmaten op (na rotatie!)
    int sw = tft.width();  // Geeft 320 op je huidige scherm
    int sh = tft.height(); // Geeft 170 op je huidige scherm

    // 4. Initialiseer de sprites volgens S.P.O.T.
    // Klok: Vaste afmetingen uit Config (150x150)
    clkSpr.createSprite(Config::clock_w, Config::clock_h);

    // Data Panelen: Dynamische breedte (320 - 150 = 170) x Vaste hoogte (150)
    int dataW = Config::get_data_width(sw);
    datSpr1.createSprite(dataW, Config::data_h);
    datSpr2.createSprite(dataW, Config::data_h);

    // Ticker: Volledige schermbreedte x Hoogte uit Config (20)
    tckSpr.createSprite(sw, Config::ticker_h);

    // Serial.printf("[DISPLAY] Sprites aangemaakt: Klok(%dx%d), Data(%dx%d), Ticker(%dx%d)\n",
    //               Config::clock_w, Config::clock_h, dataW, Config::data_h, sw, Config::ticker_h);

    // 5. Initialiseer PWM & Fade-in naar start-helderheid
    ledcAttach(Config::pin_backlight, Config::pwm_freq, Config::pwm_res);
    // updateDisplayBrightness(state.display.backlight_pwm);
}

// --- FINALISATIE NA SETUP: Alles klaarzetten en dashboard tonen ---
void finalizeSetupAndShowDashboard()
{
    // 1. Trek het gordijn dicht (Snel faden naar zwart)
    for (int b = state.display.backlight_pwm; b >= 0; b -= 10)
    {
        updateDisplayBrightness(b);
        delay(5);
    }

    manageTimeFunctions();
    int targetBrightness = (state.env.is_night_mode) ? state.display.night_brightness : state.display.day_brightness;

    // VOORWAARDE: Als we in setup of beheer zitten, doen we NIETS met sprites
    if (state.network.is_setup_mode || state.network.web_server_active)
    {
        // Alleen de helderheid regelen (zodat we de QR kunnen zien)
        updateDisplayBrightness(state.display.day_brightness);
        return;
    }

    // 2. Wis het TFT-scherm (Dolfijn weg)
    tft.fillScreen(TFT_BLACK);

    // 3. Bouw het COMPLETE dashboard op in de achtergrond (Sprites vullen)
    updateClock();          // Vult clkSpr
    manageDataPanels();     // Vult datSpr (omdat data_is_fresh nu true is)
    updateTickerSegments(); // Vult tckSpr met MODE_NORMAL

    // 4. "Atomic" Push: Stuur alles direct achter elkaar naar het scherm
    // Geen berekeningen meer hier, alleen pixels pushen!
    clkSpr.pushSprite(Config::clock_x, Config::clock_y);
    datSpr1.pushSprite(Config::data_x, Config::data_y);
    tckSpr.pushSprite(0, Config::get_ticker_y(tft.height()));

    // 5. Trek het gordijn open (Rustig faden naar de ingestelde helderheid)
    for (int b = 0; b <= targetBrightness; b += 5)
    {
        updateDisplayBrightness(b);
        delay(15);
    }
}

// --- BACKLIGHT FUNCTIES ---
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

// --- TICKER SEGMENT FUNCTIES ---
void addTickerSegment(String txt, uint16_t col)
{
    TickerSegment seg;

    // Tekst kopiëren van String naar char array
    strncpy(seg.text, txt.c_str(), sizeof(seg.text) - 1);
    seg.text[sizeof(seg.text) - 1] = '\0';

    seg.color = col;
    seg.width = tft.textWidth(seg.text);

    state.ticker_segments.push_back(seg);
}

// --- KLOK UPDATE FUNCTIE ---
void updateClock()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        // Teken de wijzers (deze functie komt uit classic_clock.cpp)
        // We geven de uren, minuten en seconden door
        renderFace(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
}

// --- DATA PANEL UPDATE FUNCTIES ---
void updateDataPaneelVandaag()
{
    uint16_t TFT_BGD = state.env.is_night_mode ? Config::bg_color_night : Config::bg_color_day;
    uint16_t TFT_GFX = state.env.is_night_mode ? Config::text_color_night : Config::gfx_color_day;

    datSpr1.fillSprite(TFT_BLACK);

    // --- 1. ICOON ---
    drawWeatherIcon(datSpr1, 10, 5, 80, state.weather.current_icon, !state.env.is_night_mode);

    // --- 2. TEMPERATUUR ---
    datSpr1.setTextFont(4);
    datSpr1.setTextColor(TFT_WHITE, TFT_BLACK);
    datSpr1.setTextDatum(TR_DATUM);
    int xPos = 88, yPos = 78;
    datSpr1.drawString(String(state.weather.temp, 1), xPos - 20, yPos);
    datSpr1.drawString("c", xPos, yPos);
    datSpr1.drawCircle(xPos - 15, yPos + 2, 3, TFT_WHITE);
    datSpr1.drawCircle(xPos - 15, yPos + 2, 4, TFT_WHITE);

    // --- 3. WINDKOMPAS ---
    int kX = 135, kY = 45, kR = 28;
    int graden = state.weather.wind_deg;
    datSpr1.drawCircle(kX, kY, kR, TFT_BGD);
    datSpr1.setTextFont(1);
    datSpr1.setTextColor(TFT_GFX);
    datSpr1.setTextDatum(MC_DATUM);
    datSpr1.drawString("N", kX, kY - kR + 6);
    datSpr1.drawString("O", kX + kR - 6, kY);
    datSpr1.drawString("Z", kX, kY + kR - 6);
    datSpr1.drawString("W", kX - kR + 6, kY);

    float rad = (graden - 90) * 0.0174533;
    float radBack = (graden + 90) * 0.0174533;
    float radSide1 = (graden) * 0.0174533;
    float radSide2 = (graden + 180) * 0.0174533;

    int tipX = kX + kR * cos(rad), tipY = kY + kR * sin(rad);
    int tailX = kX + kR * cos(radBack), tailY = kY + kR * sin(radBack);
    int s1x = kX + 5 * cos(radSide1), s1y = kY + 5 * sin(radSide1);
    int s2x = kX + 5 * cos(radSide2), s2y = kY + 5 * sin(radSide2);

    datSpr1.fillTriangle(tipX, tipY, s1x, s1y, kX, kY, TFT_GOLD);
    datSpr1.drawTriangle(tipX, tipY, s2x, s2y, kX, kY, TFT_GOLD);
    datSpr1.fillTriangle(tailX, tailY, s1x, s1y, kX, kY, TFT_DARKGREY);
    datSpr1.drawTriangle(tailX, tailY, s2x, s2y, kX, kY, TFT_DARKGREY);

    datSpr1.setTextFont(2);
    datSpr1.setTextColor(TFT_SKYBLUE);
    int bft = getBeaufort(state.weather.wind_speed);
    datSpr1.drawString(String(bft) + " Bft", kX, kY + kR + 18);

    // --- 4. EXTRA INFO ---
    // datSpr1.setTextFont(2);
    // datSpr1.setTextDatum(BL_DATUM);
    // datSpr1.setTextColor(TFT_LIGHTGREY);
    // datSpr1.drawString("V: " + String((int)state.weather.humidity) + "% | UV: " + String(state.weather.uvi, 1), 5, 145);

    // Voorbeeld voor in je sprite/render:
    int xMidden = 10;
    int yMidden = 136;

    if (state.env.baro_trend == 1)
    { // Punten: (top), (links-onder), (rechts-onder)
        datSpr1.fillTriangle(xMidden, yMidden - 6, xMidden - 4, yMidden - 2, xMidden + 4, yMidden - 2, TFT_GREEN);
    }
    else if (state.env.baro_trend == -1)
    { // Punten: (bodem), (links-boven), (rechts-boven)
        datSpr1.fillTriangle(xMidden, yMidden + 6, xMidden - 4, yMidden + 2, xMidden + 4, yMidden + 2, TFT_RED);
    }
    else
    { // Stabiel: klein streepje of niks
        datSpr1.drawFastHLine(xMidden - 5, yMidden, 12, TFT_LIGHTGREY);
    }

    datSpr1.setTextDatum(BL_DATUM); // Bottom Left
    datSpr1.setTextColor(TFT_GOLD);
    char baroBuffer[20];
    datSpr1.drawString(getBaroText(baroBuffer, sizeof(baroBuffer)), xMidden + 12, yMidden + 8, 2); // Tekst naast de pijltjes

    drawWifiIndicator(146, 130, 13);
    if (state.env.is_alert_active)
    {
        drawISOAlert(120, 122, 20, 0, state.env.is_alert_active);
    }

    // --- 5. DE RELATIEVE HORIZON ---
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return;

    int tlX_center = 85, tlY = 118;
    float pixelsPerUur = Config::distance_per_hour; // Eventueel SECRET_HORIZON_PIXELS_PER_HOUR uit config
    uint16_t shadowCol = state.env.is_night_mode ? 0x2104 : TFT_DARKGREY;
    float phase = state.weather.today.moon_phase;
    float r = 5;

    float nuDecimal = timeinfo.tm_hour + (timeinfo.tm_min / 60.0);
    float distZonOp = state.env.sunrise_local - nuDecimal;
    float distZonOnder = state.env.sunset_local - nuDecimal;
    float distMaanOp = state.env.moonrise_local - nuDecimal;
    float distMaanOnder = state.env.moonset_local - nuDecimal;

    // Correctie voor de datumgrens (als de maan morgenochtend pas ondergaat)
    if (distMaanOnder < -12.0)
        distMaanOnder += 24.0;
    if (distMaanOp < -12.0)
        distMaanOp += 24.0;

    datSpr1.drawFastHLine(5, tlY + 1, 170, state.env.is_night_mode ? 0x2104 : TFT_DARKGREY);

    int xZonOp = tlX_center + (int)(distZonOp * pixelsPerUur);
    int xZonOnder = tlX_center + (int)(distZonOnder * pixelsPerUur);
    int xMaanOp = tlX_center + (int)(distMaanOp * pixelsPerUur);
    int xMaanOnder = tlX_center + (int)(distMaanOnder * pixelsPerUur);

    // Maan op en onder tekenen als ze binnen het zichtbare bereik vallen (0-170 pixels)
    // Hulpvariabele voor de schaduwkleur (matcht met je horizonlijn)
    if (xMaanOp > 0 && xMaanOp < 170)
    {
        // 1. De basis (volledige maan in goud/wit)
        datSpr1.fillCircle(xMaanOp, tlY + 1, r, 0xDEFB);

        // 2. De schaduw (vormt de fase)
        if (phase < 0.5)
        {
            // Wassend: schaduw schuift van rechts naar links
            int offset = map(phase * 1000, 0, 500, r * 2, 0);
            datSpr1.fillCircle(xMaanOp + offset, tlY + 1, r, shadowCol);
        }
        else
        {
            // Afnemend: schaduw schuift van links naar rechts
            int offset = map(phase * 1000, 500, 1000, 0, r * 2);
            datSpr1.fillCircle(xMaanOp - offset, tlY + 1, r, shadowCol);
        }
    }
    if (xMaanOnder > 85 && xMaanOnder < 170)
    {
        // datSpr1.fillCircle(xMaanOnder, tlY + 1, 4, TFT_CYAN);
        // 1. De basis (volledige maan in goud/wit)
        datSpr1.fillCircle(xMaanOnder, tlY + 1, r, TFT_DARKCYAN);

        // 2. De schaduw (vormt de fase)
        if (phase < 0.5)
        {
            // Wassend: schaduw schuift van rechts naar links
            int offset = map(phase * 1000, 0, 500, r * 2, 0);
            datSpr1.fillCircle(xMaanOnder + offset, tlY + 1, r - 1, shadowCol);
        }
        else
        {
            // Afnemend: schaduw schuift van links naar rechts
            int offset = map(phase * 1000, 500, 1000, 0, r * 2);
            datSpr1.fillCircle(xMaanOnder - offset, tlY + 1, r - 1, shadowCol);
        }
    }

    // Zon op en onder tekenen als ze binnen het zichtbare bereik vallen (0-170 pixels)
    // als laatste tekenen om overlapping van de maanschaduw te voorkomen.
    if (xZonOp > 0 && xZonOp < 170)
    {
        datSpr1.fillCircle(xZonOp, tlY + 1, r + 1, TFT_WHITE); // De witte 'gloed' rand
        datSpr1.fillCircle(xZonOp, tlY + 1, r, TFT_YELLOW);    // De gele kern
        datSpr1.setTextDatum(BC_DATUM);
        datSpr1.setTextColor(TFT_LIGHTGREY);
        datSpr1.setTextFont(1);
        if (xZonOp > 95)
        {
            datSpr1.drawString(String(state.env.sunrise_str), xZonOp, tlY - 5);
        }
    }
    if (xZonOnder > 85 && xZonOnder < 170)
    {
        datSpr1.fillCircle(xZonOnder, tlY + 1, r, TFT_ORANGE);
        datSpr1.setTextDatum(BC_DATUM);
        datSpr1.setTextColor(TFT_LIGHTGREY);
        datSpr1.setTextFont(1);
        if (xZonOnder > 95)
        {
            datSpr1.drawString(String(state.env.sunset_str), xZonOnder, tlY - 5);
        }
    }

    datSpr1.fillTriangle(tlX_center, tlY - 2, tlX_center - 4, tlY - 8, tlX_center + 4, tlY - 8, TFT_WHITE);
}

// --- UPDATE FUNCTIE VOOR HET VOORSPELLINGS-PANEEL (3 DAGEN) ---
void updateDataPaneelForecast()
{
    datSpr2.fillSprite(TFT_BLACK); // We gebruiken datSpr voor beide panelen
    datSpr2.setTextColor(TFT_WHITE, TFT_BLACK);

    for (int i = 0; i < 3; i++)
    {
        int yPos = i * 50;
        auto &f = state.weather.forecast[i];

        time_t rawtime = (time_t)f.dt;
        struct tm *ti = localtime(&rawtime);

        char datumBuf[12];
        strftime(datumBuf, sizeof(datumBuf), "%d-%m", ti); // "28-01"

        datSpr2.setTextFont(2);
        datSpr2.setTextDatum(TR_DATUM);
        datSpr2.setTextColor(TFT_WHITE, TFT_BLACK);
        datSpr2.drawString(dagNamen[ti->tm_wday], 32, yPos + 10);

        datSpr2.setTextFont(1);
        datSpr2.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        datSpr2.drawString(datumBuf, 33, yPos + 30);

        datSpr2.setTextDatum(TR_DATUM);
        datSpr2.setTextFont(2);
        datSpr2.setTextColor(TFT_GOLD, TFT_BLACK);
        String tMinMax = /*String(f.temp_min, 1) + " - " + */ String(f.temp_max, 1);
        datSpr2.drawString(tMinMax, 169 - 12, yPos + 12);
        datSpr2.drawString("c", 169, yPos + 10);
        datSpr2.drawCircle(165 - 8, yPos + 11, 2, TFT_GOLD);

        datSpr2.setTextFont(1);
        datSpr2.setTextColor(TFT_SKYBLUE, TFT_BLACK);
        datSpr2.drawString(f.description, 169, yPos + 34);

        drawWeatherIcon(datSpr2, 37, yPos + 5, 40, f.icon_id, true);

        if (i < 2)
            datSpr2.drawFastHLine(0, yPos + 49, 170, 0x2104);
    }
}

// --- TEKENFUNCTIE VOOR QR-CODE: Gebruikt de qrcode library om een QR-code te genereren en direct op het TFT-scherm te tekenen. ---
void drawQRCodeOnTFT(const char *data, int x, int y, int scale)
{
    QRCode qrcode;
    // Version 4 is iets groter (33x33), ideaal voor de WiFi-string
    uint8_t qrcodeData[qrcode_getBufferSize(4)];
    qrcode_initText(&qrcode, qrcodeData, 4, 0, data);

    // Teken een wit kader voor beter scannen (Quiet Zone)
    tft.fillRect(x - 5, y - 5, (qrcode.size * scale) + 10, (qrcode.size * scale) + 10, TFT_WHITE);

    for (uint8_t qy = 0; qy < qrcode.size; qy++)
    {
        for (uint8_t qx = 0; qx < qrcode.size; qx++)
        {
            uint16_t color = qrcode_getModule(&qrcode, qx, qy) ? TFT_BLACK : TFT_WHITE;
            tft.fillRect(x + (qx * scale), y + (qy * scale), scale, scale, color);
        }
    }
}

// ---   TEKENFUNCTIES ---

// Tekent een WiFi-indicator met 4 bars, waarbij de kleur en het aantal bars afhankelijk zijn van het gescande signaal (RSSI).
// De indicator heeft ook een speciale "stand-by" kleur wanneer we in On-Demand mode zitten zonder verbinding.
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
            datSpr1.fillRoundRect(x + (i * 6), y + (h - (i * 4) - 4), 4, (i * 4) + 4, 1, color);
        }
    }
    else
    { // --- STATUS: UIT ---
        uint16_t statusColor;
        if (state.network.wifi_mode == 1)
        { // Mode: On-Demand (Wacht op update slot)
            statusColor = standbyGreen;
        }
        else
        { // Mode: Eco-Nacht of uit
            statusColor = state.env.icon_base_color;
        }

        for (int i = 0; i < 4; i++)
        {
            datSpr1.fillRoundRect(x + (i * 6), y + (h - (i * 4) - 4), 4, (i * 4) + 4, 1, statusColor);
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
    datSpr1.fillRect(x /*- (size / 2)*/ - 2, y /*- 2*/, size + 4, size + 4, TFT_BLACK ); // Groot zwart vierkant als achtergrond
    datSpr1.fillTriangle(x + size / 2, y, x, y + size, x + size, y + size, TFT_BLACK);

    // 2. Het gekleurde binnenvlak (Dit wordt nu 0x2104 of TFT_GREENYELLOW)
    int offset = size / 10;
    if (offset < 2)
        offset = 2;
    datSpr1.fillTriangle(x + size / 2, y + (offset * 1.5), x + offset, y + size - (offset / 2), x + size - offset, y + size - (offset / 2), signColor);

    // 3. Teken het '!' teken alleen als er echt een alarm is
    // if (isActuallyActive)
    // {
        int barWidth = size / 6;
        int barHeight = size / 2.4;
        datSpr1.fillRoundRect(x + (size / 2) - (barWidth / 2), y + (size / 3), barWidth, barHeight, 1, TFT_BLACK);
        datSpr1.fillCircle(x + (size / 2), 1 + y + size - (size / 4), size / 10, TFT_BLACK);
    // }
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
        { // Muted: Subtiel 'hartslag' effect (knippert heel kort elke 3 seconden)
            digitalWrite(Config::pin_fingerprint_led, (millis() % 3000 < 50));
        }
        else
        { // Actief: Opvallend knipperen (Safety Car tempo)
            digitalWrite(Config::pin_fingerprint_led, (millis() % 1000 < 500));
        }
    }
    else
    {
        digitalWrite(Config::pin_fingerprint_led, LOW); // Ruststand
    }
}

void handleTouchLadder()
{
    int touchVal = touchRead(13);
    static unsigned long touchStart = 0;
    static bool isTouching = false;
    static unsigned long lastLog = 0;
    static int lastStatus = -1; // Voor het forceren van een update bij statusverandering

    // --- 1. AANRAKING DETECTIE ---
    if (touchVal < 55)
    {
        if (!isTouching)
        {
            touchStart = millis(); // Dit mag maar EÉN keer gebeuren bij de eerste aanraking!
            isTouching = true;
            state.env.is_touching = true;
            Serial.printf("[TOUCH] Start meting...\n");
        }

        state.display.touch_indicator_color = TFT_BLACK; // Reset de indicatorkleur bij elke update, zodat we alleen feedback zien als we daadwerkelijk aan het drukken zijn
        unsigned long duration = millis() - touchStart;

        // // DEBUG (alleen als we drukken)
        // if (millis() - lastLog > 500)
        // {
        //     Serial.printf("[DEBUG-TOUCH] Val: %d | Dur: %lu ms | Server: %s\n",
        //                   touchVal, duration, state.network.web_server_active ? "AAN" : "UIT");
        //     lastLog = millis();
        // }

        // FEEDBACK (Alleen LED, GEEN acties!)
        updateTouchLedFeedback(duration);
    }
    else if (isTouching)
    { // --- VINGER ERAF: Hier gebeurt de actie ---
        unsigned long eindDuur = millis() - touchStart;
        state.display.touch_indicator_color = TFT_BLACK; // Reset de indicatorkleur

        // 1. ZONE: 2 - 5 SEC (Context acties)
        if (eindDuur >= 2000 && eindDuur < 5000)
        {
            // Als de server aan staat (Setup of Beheer), sluiten we nu ALTIJD alles af
            if (state.network.web_server_active || state.network.is_setup_mode)
            {
                // deactivateWiFiAndServer();
                powerDownWiFi();
                finalizeUIAfterSetup();
                state.network.is_setup_mode = false;
            }
            else if (state.display.show_system_monitor)
            {
                // Als we in de systeemmonitor zitten, sluiten we die netjes af
                // state.display.show_system_monitor = false;
                // state.display.show_sm_bg_drawn = false;
                state.network.pending_restart = true; // Forceren van een herstart om alle systeemmonitor variabelen netjes te resetten
                return;                               // Direct terugkeren, zodat we de rest van de acties NIET uitvoeren
                // (zoals het forceren van een update of het refreshen van de panelen),
                // omdat we in een soort 'tussenstatus' zitten waarbij sommige variabelen al gereset zijn en andere nog niet.
                // De herstart zal dit netjes oplossen zonder dat we risico lopen op onvoorziene bugs door gedeeltelijke resets.
                // tft.fillScreen(TFT_BLACK);
            }
            else
            {
                // Normale werking: toggle alert display
                lastStatus = -1; // Forceren van een update in de volgende loop, zodat we direct visuele feedback krijgen van de verandering
                state.display.force_alert_display = !state.display.force_alert_display;
                state.display.alert_show_until = state.display.force_alert_display ? millis() + Config::ten_min_timeout : 0;
            }
            state.display.force_ticker_refresh = true;
            Config::forceFirstWeatherUpdate = false;
        }

        // 2. ZONE: 5 - 8 SEC (De Generaal: WebConfig)
        else if (eindDuur >= 5000 && eindDuur < 8000)
        {
            if (!state.network.web_server_active)
            {
                tft.fillScreen(TFT_BLACK);
                activateWiFiAndServer(); // Start de server pas NU
                // state.display.force_ticker_refresh = true;
            }
        }

        // 3. ZONE: 8 - 11 SEC (De Machinekamer: Systeem Status)
        else if (eindDuur >= 8000 && eindDuur < 11000)
        {
            state.display.show_system_monitor = !state.display.show_system_monitor;
            // lastStatus = -1; // Forceren van een update in de volgende loop, zodat we direct visuele feedback krijgen van de verandering

            if (state.display.show_system_monitor)
            {
                tft.fillScreen(TFT_BLACK);              // Scherm schoonvegen voor het dashboard
                state.display.show_sm_bg_drawn = false; // We gaan naar de systeemmonitor, dus we moeten het achtergrondscherm tekenen
            }
            else
            {
                state.display.force_ticker_refresh = false; // Netjes terugkeren
            }
        }

        // --- RESET VOOR DE VOLGENDE KEER ---
        digitalWrite(Config::pin_fingerprint_led, state.network.web_server_active ? HIGH : LOW);
        isTouching = false;
        touchStart = 0;
        state.env.is_touching = false;
        state.display.touch_indicator_color = TFT_BLACK;
        state.display.show_sm_bg_drawn = false;

        // Update direct het datapaneel voor visuele feedback
        // Update alleen de panelen als we NIET in een speciale modus zitten
        if (!state.network.is_setup_mode && !state.network.web_server_active && !state.display.show_system_monitor)
        {
            manageDataPanels();
        }
    }
}

// even kijken of we dit nog nodig hebben of gebruiken om de status van handleTouchLadder te ondersteunen
void manageStatusLed()
{
    // Als we aan het drukken zijn, bemoeit deze functie zich nergens mee
    if (state.env.is_touching)
        return;

    if (state.network.web_server_active)
    {
        // Server actief: Constant aan met een kleine 'heartbeat' uit
        digitalWrite(Config::pin_fingerprint_led, (millis() % 5000 > 100));
    }
    else if (state.env.is_alert_active && !state.env.alert_muted)
    {
        digitalWrite(Config::pin_fingerprint_led, (millis() % 1000 < 500));
    }
    else
    {
        digitalWrite(Config::pin_fingerprint_led, LOW);
    }
}

void updateTouchLedFeedback(unsigned long duration)
{
    if (duration < 2000)
    {
        digitalWrite(Config::pin_fingerprint_led, (millis() % 400 < 100)); // Klaarstaan (kort flitsje)
        state.display.touch_indicator_color = TFT_YELLOW;
    }
    else if (duration < 5000)
    {
        digitalWrite(Config::pin_fingerprint_led, (millis() % 200 < 100)); // Snel (Actie/Alert zone)
        state.display.touch_indicator_color = TFT_MAGENTA;
    }
    else if (duration < 8000)
    {
        digitalWrite(Config::pin_fingerprint_led, HIGH); // Constant (WebConfig zone)
        state.display.touch_indicator_color = TFT_CYAN;
    }
    else if (duration < 11000)
    {
        // SOS Morse Feedback (Systeem Status zone)
        int cycle = (millis() % 1500);
        bool ledOn = (cycle < 300 && cycle % 100 < 50) ||                  // S
                     (cycle >= 300 && cycle < 900 && cycle % 300 < 200) || // O
                     (cycle >= 900 && cycle < 1200 && cycle % 100 < 50);   // S
        digitalWrite(Config::pin_fingerprint_led, ledOn);
        state.display.touch_indicator_color = TFT_RED;
    }
    else
    {
        digitalWrite(Config::pin_fingerprint_led, LOW); // Abort
        state.display.touch_indicator_color = TFT_BLACK;
    }
}

void manageAlertTimeout()
{
    if (!state.env.alert_muted && millis() > Config::alert_timeout)
    {
        state.env.alert_muted = true;         // Terug naar rustige stand
        state.display.ticker_x = tft.width(); // Reset ticker weer
        updateTickerSegments();
        Serial.printf("[ALARM] Auto-timeout: Ticker terug naar normaal.\n");
    }
}

void evaluateSystemSafety()
{
    float temp = state.env.case_temp;

    // 1. ABSOLUTE NOODSTOP (Altijd als eerste checken)
    if (temp > Config::system_temp_threshold)
    {
        if (state.env.health != HEALTH_CRITICAL)
        {
            digitalWrite(Config::pin_buck_enable, HIGH); // Buck UIT
            state.env.health = HEALTH_CRITICAL;
            Serial.printf("[SAFETY] EMERGENCY: Buck gedeactiveerd!\n");
        }
        return; // Stop hier, we zijn in lockdown
    }

    // 2. HERSTEL VAN NOODSTOP (Hysteresis)
    // We komen pas uit CRITICAL als we onder de 60 graden zijn
    if (state.env.health == HEALTH_CRITICAL)
    {
        if (temp < Config::system_temp_hysteresis)
        {
            digitalWrite(Config::pin_buck_enable, LOW); // Buck weer AAN
            state.env.health = HEALTH_WARNING;          // We zakken een niveau

            // Reanimeer het scherm
            setupDisplay();
            Serial.printf("[SAFETY] Herstel: Scherm gereanimeerd.\n");
        }
        else
        {
            return; // Nog te warm, blijf in CRITICAL (Buck uit)
        }
    }

    // 3. NORMALE MONITORING (Warning & OK)
    if (temp > Config::system_temp_warning)
    {
        state.env.health = HEALTH_WARNING;
    }
    else if (temp < Config::system_temp_hysteresis)
    { // Hysteresis van 5 graden voor rustig beeld
        state.env.health = HEALTH_OK;
    }
}

void finalizeUIAfterSetup()
{
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
