/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Leeftijd berekenen op basis van geboortedatum
 */

#pragma once
#include <Arduino.h>

struct Leeftijd
{
    int jaren;
    int maanden;
    int dagen;
    String fact;
};

// Functie die de berekening doet op basis van de huidige systeemtijd
Leeftijd berekenLeeftijd(int geboorteDag, int geboorteMaand, int geboorteJaar);
void toonPersoonlijkeInfo();
