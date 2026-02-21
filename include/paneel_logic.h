
#pragma once
#include <Arduino.h>

// Forward declaration: We vertellen de compiler alleen DAT er een 
// klasse bestaat die TFT_eSprite heet, zonder de hele library te laden.
class TFT_eSprite; 

// De functie gebruikt nu alleen de referentie (pointers)
void performTransition(TFT_eSprite* oldSpr, TFT_eSprite* newSpr);

extern void updateDataPaneelVandaag();
extern void updateDataPaneelForecast();
extern void showSetupInstructionPanel();