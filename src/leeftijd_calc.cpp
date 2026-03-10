/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Leeftijd berekenen op basis van geboortedatum
 */

#include "leeftijd_calc.h"
#include "global_data.h"
#include "helpers.h"
#include "secret.h"
#include <time.h>

#include <Arduino.h>
#include "global_data.h"

// 1. De rauwe rekenaar (vult de buffer)
void vulEasterEggTekst(char *buffer, size_t bufferSize, int gDag, int gMaand, int gJaar, int Variant)
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        snprintf(buffer, bufferSize, "TIJD FOUT");
        return;
    }

    int j = (timeinfo.tm_year + 1900) - gJaar;
    int m = (timeinfo.tm_mon + 1) - gMaand;
    int d = timeinfo.tm_mday - gDag;
    if (d < 0)
    {
        m--;
        d += 30;
    }
    if (m < 0)
    {
        j--;
        m += 12;
    }

    float totaalDagen = (j * 365.25) + (m * 30.44) + d;
    static int factCounter = 0;
    int variant;
  
    variant = (factCounter++) % 13;

    // Gebruik snprintf om direct in de buffer te schrijven
    if (variant == 0)

        snprintf(buffer, bufferSize, "JE BENT %dj, %dm en %dd OUD!", j, m, d);

    else if (variant == 1)

        snprintf(buffer, bufferSize, "JE REISDE %ld MILJOEN KM DOOR DE RUIMTE!", (long)((totaalDagen / 365.25) * 940));
        
    else if (variant == 2)
    {
        int manen = (int)(totaalDagen / 29.53);
        snprintf(buffer, bufferSize, "JE HEBT DE MAAN AL %d KEER ZIEN VERANDEREN!", manen);
    }
    else if (variant == 3)
    {
        long miljoenSlagen = (long)(totaalDagen * 24 * 60 * 75 / 1000000);
        snprintf(buffer, bufferSize, "JE HART HEEFT AL %ld MILJOEN KEER GESLAGEN!", miljoenSlagen);
    }
    else if (variant == 4)
    {
        snprintf(buffer, bufferSize, "JE HEBT DE ZON AL %d KEER ZIEN OPKOMEN!", (int)totaalDagen);
    }
    else if (variant == 5)
    {
        snprintf(buffer, bufferSize, "OP DE PLANEET MARS BEN JE PAS %d JAAR OUD!", (int)(totaalDagen / 687.0));
    }
    else if (variant == 6)
    {
        long miljoenAdem = (long)(totaalDagen * 24 * 60 * 14 / 1000000);
        snprintf(buffer, bufferSize, "JE HEBT AL %ld MILJOEN KEER ADEMGEHAALD!", miljoenAdem);
    }
    else if (variant == 7)
    {
        long miljardKmMelkweg = (long)(totaalDagen * 24 * 0.828);
        snprintf(buffer, bufferSize, "JE BENT AL %ld MILJOEN KM DOOR DE MELKWEG GESURFT!", miljardKmMelkweg);
    }
    else if (variant == 8)
    {
        int urenGedroomd = (int)(totaalDagen * 2);
        snprintf(buffer, bufferSize, "JE HEBT AL MEER DAN %d UUR IN DROMENLAND DOORGEBRACHT!", urenGedroomd);
    }
    else if (variant == 9)
    {
        long miljoenKnippers = (long)(totaalDagen * 24 * 60 * 17 / 1000000);
        snprintf(buffer, bufferSize, "JE HEBT JE OGEN AL %ld MILJOEN KEER DICHTGEKNIPPERD!", miljoenKnippers);
    }
    else if (variant == 10)
    {
        long duizendLiter = (long)(totaalDagen * 11.0);
        snprintf(buffer, bufferSize, "JE HEBT AL %ld DUIZEND LITER LUCHT INGEADEMD!", duizendLiter);
    }
    else if (variant == 11)
    {
        long miljardKmLicht = (long)(totaalDagen * 25.9);
        snprintf(buffer, bufferSize, "ZONLICHT HEEFT SINDS JE GEBOORTE %ld MILJARD KM GEREISD!", miljardKmLicht);
    }

    else if (variant == 12)
        snprintf(buffer, bufferSize, "JE PRODUCEERDE %ld KM HAAR!", (long)(totaalDagen * 35.0 / 1000.0));

    // Serial.printf("[EASTER-EGG] Fact Counter: %d, Variant: %d\n", factCounter, variant);
}

// 2. De Wrapper (haalt data uit de State en bouwt de zin)
bool vulGepersonaliseerdFeitje(char *targetBuffer, size_t bufferSize)
{
    if (state.user.name.length() == 0 || state.user.dob.length() < 10)
        return false;

    int gJ, gM, gD;
    if (sscanf(state.user.dob.c_str(), "%d-%d-%d", &gJ, &gM, &gD) != 3)
        return false;

    char factTmp[90]; // Tijdelijke buffer voor het feitje zelf
    vulEasterEggTekst(factTmp, sizeof(factTmp), gD, gM, gJ, -1); // Use -1 to get random variant

    // Bouw de uiteindelijke zin in de TickerSegment buffer
    snprintf(targetBuffer, bufferSize, " ;P %s, %s :D ", state.user.name.c_str(), factTmp);
    return true;
}
