/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Clasic Clock rendering logic
 */

#include "clasic_clock.h"
#include "daynight.h"
#include "env_sensors.h"
#include "global_data.h"
#include "helpers.h"
#include "secret.h"
#include <TFT_eSPI.h>

extern TFT_eSPI tft; // Verwijs naar de tft in helpers.cpp
extern TFT_eSprite clkSpr; // Verwijs naar de sprite in helpers.cpp

// Constanten voor de klok-wiskunde
#define CLOCK_R 73.0f // Straal (150/2-2)
#define H_HAND_LENGTH (CLOCK_R / 2.0f)
#define M_HAND_LENGTH (CLOCK_R / 1.4f)
#define S_HAND_LENGTH (CLOCK_R / 1.3f)
#define SECOND_FG TFT_RED
#define DEG2RAD 0.0174532925

// Functie voor coördinaten berekening
void getCoord(int16_t x, int16_t y, float* xp, float* yp, int16_t r, float a)
{
    float sx1 = cos((a - 90) * DEG2RAD);
    float sy1 = sin((a - 90) * DEG2RAD);
    *xp = sx1 * r + x;
    *yp = sy1 * r + y;
}

void renderFace(int h, int m, int s)
{
    float s_angle = s * 6.0;
    float m_angle = m * 6.0 + s_angle / 60.0;
    float h_angle = (h % 12) * 30.0 + m_angle / 12.0;

    // Kleuren bepalen op basis van de centrale state
    uint16_t CLK_BG = state.env.is_night_mode ? Config::bg_color_night : Config::bg_color_day;
    uint16_t CLK_FG = state.env.is_night_mode ? Config::gfx_color_night : Config::gfx_color_day;
    uint16_t LBL_FG = state.env.is_night_mode ? Config::text_color_day : Config::text_color_day;

    // 1. Wis de klok sprite intern
    clkSpr.fillSprite(TFT_BLACK);

    // 2. Teken de wijzerplaat cirkel
    clkSpr.fillCircle(CLOCK_R, CLOCK_R, CLOCK_R, CLK_BG);

    // 3. Teken de cijfers (1-12 of 13-24)
    clkSpr.setTextColor(CLK_FG, CLK_BG);
    clkSpr.setTextDatum(MC_DATUM);
    clkSpr.setTextFont(1);
    constexpr uint32_t dialOffset = CLOCK_R - 14;
    int amPmOffset = (h >= 12) ? 12 : 0;

    for (uint32_t i = 1; i <= 12; i++) {
        float xp, yp;
        getCoord(CLOCK_R, CLOCK_R, &xp, &yp, dialOffset, i * 30.0);
        clkSpr.drawNumber(i + amPmOffset, xp, yp + 2);
    }

    // 4. Teken Sensor Data (Bovenzijde as)
    clkSpr.setTextFont(2);
    clkSpr.setTextColor(LBL_FG, CLK_BG);

    String tAlleen = String(state.env.temp_local, 1); // Temperatuur met 1 decimaal
    int xPos = CLOCK_R - 8;
    int yPos = 52 + 8;
    clkSpr.drawString(tAlleen, xPos - 25, yPos); // We schuiven iets naar links voor de C
    clkSpr.drawString("c", xPos, yPos);
    clkSpr.drawCircle(xPos - 7, yPos - 4, 2, LBL_FG);

    clkSpr.setTextDatum(TR_DATUM);
    clkSpr.drawString(String(state.env.hum_local, 0), CLOCK_R + 31, 52); // Vocht rechts
    clkSpr.setTextDatum(TL_DATUM);
    clkSpr.drawString("%", CLOCK_R + 32, 52);

    // Luchtdruk (Y=65)
    clkSpr.setTextDatum(MC_DATUM);
    char pBuf[16];
    sprintf(pBuf, "%.0f hPa", state.env.press_local);
    clkSpr.drawString(pBuf, CLOCK_R, 95);

    // 6. Teken de wijzers
    float xp, yp;
    // Minuten
    getCoord(CLOCK_R, CLOCK_R, &xp, &yp, M_HAND_LENGTH, m_angle);
    clkSpr.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 5.0f, CLK_FG);
    clkSpr.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 2.0f, CLK_BG);
    // Uren
    getCoord(CLOCK_R, CLOCK_R, &xp, &yp, H_HAND_LENGTH, h_angle);
    clkSpr.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 5.0f, CLK_FG);
    clkSpr.drawWideLine(CLOCK_R, CLOCK_R, xp, yp, 2.0f, CLK_BG);
    // As
    clkSpr.fillCircle(CLOCK_R, CLOCK_R, 4, CLK_FG);
    // Seconden (Wedge)
    getCoord(CLOCK_R, CLOCK_R, &xp, &yp, S_HAND_LENGTH, s_angle);
    clkSpr.drawWedgeLine(CLOCK_R, CLOCK_R, xp, yp, 2.5, 1.0, SECOND_FG);

    // 7. PUSH naar scherm (X=0, Y=0)
    clkSpr.pushSprite(0, 0);
}
