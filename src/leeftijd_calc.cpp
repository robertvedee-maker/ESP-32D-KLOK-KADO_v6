/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Leeftijd berekenen op basis van geboortedatum
 */

#include "leeftijd_calc.h"
#include "global_data.h"
#include "helpers.h"
#include "secret.h"
#include <time.h>


// Wrapper functie voor gebruik met de State-data
String getGepersonaliseerdFeitje() {
    // 1. De 'veiligheids-check': alleen uitvoeren als er echt data is
    if (state.user.name == "" || state.user.dob == "" || state.user.dob.length() < 10) {
        return ""; // Geef niks terug, de ticker slaat dit segment dan over
    }

    // 2. Parse de YYYY-MM-DD string (bijv "1985-05-20")
    int gJaar  = state.user.dob.substring(0, 4).toInt();
    int gMaand = state.user.dob.substring(5, 7).toInt();
    int gDag   = state.user.dob.substring(8, 10).toInt();

    // Extra veiligheid: toInt() geeft 0 bij ongeldige tekst
    if (gJaar == 0 || gMaand == 0 || gDag == 0) return "";

    // 3. Bereken de leeftijd en kies een willekeurig feitje
    Leeftijd res = berekenLeeftijd(gDag, gMaand, gJaar);

    // 4. Bouw de uiteindelijke string
    return " ;P " + state.user.name + ", " + res.fact + " :D ";
}

// De echte berekeningsfunctie, los van de State
Leeftijd berekenLeeftijd(int gDag, int gMaand, int gJaar)
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
        return {0, 0, 0, ""}; // Error-return

    // 1. Basis leeftijd (jouw bestaande code)
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

    // ... binnen de berekenLeeftijd functie ...
    float totaalDagen = (j * 365.25) + (m * 30.44) + d;

    int variant = random(timeinfo.tm_sec % 13); // We breiden uit naar 13 smaken!
    String fact = "";

    if (variant == 0)
    {
        fact = "JE BENT VANDAAG EXACT " + String(j) + "j, " + String(m) + "m en " + String(d) + "d OUD!";
    }
    else if (variant == 1)
    {
        // Aarde legt 940 miljoen km per jaar af in haar baan om de zon
        long miljardKm = (long)((totaalDagen / 365.25) * 940);
        fact = "JE HEBT AL " + String(miljardKm) + " MILJOEN KM DOOR DE RUIMTE GEREISD!";
    }
    else if (variant == 2)
    {
        // Maancyclus is 29.53 dagen
        int manen = (int)(totaalDagen / 29.53);
        fact = "JE HEBT DE MAAN AL " + String(manen) + " KEER ZIEN VERANDEREN!";
    }
    else if (variant == 3)
    {
        // Hartslagen (gemiddeld 75 p/m)
        long miljoenSlagen = (long)(totaalDagen * 24 * 60 * 75 / 1000000);
        fact = "JE HART HEEFT AL " + String(miljoenSlagen) + " MILJOEN KEER GESLAGEN!";
    }
    else if (variant == 4)
    {
        // De aarde draait om haar as (1 keer per dag)
        fact = "JE HEBT DE ZON AL " + String((int)totaalDagen) + " KEER ZIEN OPKOMEN!";
    }
    else if (variant == 5)
    {
        // Mars jaar is 687 dagen
        fact = "OP DE PLANEET MARS BEN JE PAS " + String((int)(totaalDagen / 687.0)) + " JAAR OUD!";
    }
    else if (variant == 6)
    {
        // Ademhaling (gemiddeld 12-16 keer per minuut)
        long miljoenAdem = (long)(totaalDagen * 24 * 60 * 14 / 1000000);
        fact = "JE HEBT AL " + String(miljoenAdem) + " MILJOEN KEER ADEMGEHAALD!";
    }
    else if (variant == 7)
    {
        // De Melkweg draait! Ons zonnestelsel beweegt met 828.000 km/u
        long miljardKmMelkweg = (long)(totaalDagen * 24 * 0.828); // in miljoenen km
        fact = "JE BENT AL " + String(miljardKmMelkweg) + " MILJOEN KM DOOR DE MELKWEG GESURFT!";
    }
    else if (variant == 8)
    {
        // Dromen: Gemiddeld 2 uur per nacht (REM slaap)
        int urenGedroomd = (int)(totaalDagen * 2);
        fact = "JE HEBT AL MEER DAN " + String(urenGedroomd) + " UUR IN DROMENLAND DOORGEBRACHT!";
    }
    else if (variant == 9)
    {
        // Knipperen: Gemiddeld 15-20 keer per minuut
        long miljoenKnippers = (long)(totaalDagen * 24 * 60 * 17 / 1000000);
        fact = "JE HEBT JE OGEN AL " + String(miljoenKnippers) + " MILJOEN KEER DICHTGEKNIPPERD!";
    }
    if (variant == 10)
    {
        // Atmosfeer / Lucht (Link met je BMP280 passie)
        long miljoenLiter = (long)(totaalDagen * 11.000 / 1000.0); // We rekenen in duizenden liters
        fact = "JE HEBT AL " + String(miljoenLiter) + " DUIZEND LITER LUCHT INGEADEMD!";
    }
    else if (variant == 11)
    {
        // Lichtreiziger (Zon-calculatie link)
        // Licht reist ~26 miljard km per dag. 
        long miljardKmLicht = (long)(totaalDagen * 25.9); 
        fact = "ZONLICHT HEEFT SINDS JE GEBOORTE " + String(miljardKmLicht) + " MILJARD KM GEREISD!";
    }
    else if (variant == 12)
    {
        // De Haar-fabriek (De favoriet!)
        // Gemiddeld 0.35mm per dag per haar. 
        // Bij 100.000 haren is dat 35 meter per dag in totaal!
        long kmHaar = (long)(totaalDagen * 35.0 / 1000.0);
        fact = "JE HEBT IN TOTAAL AL " + String(kmHaar) + " KILOMETER HAAR GEPRODUCEERD!";
    }


    return {j, m, d, fact};
}


