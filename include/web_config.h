#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// De server als extern zodat helpers.cpp er ook bij kan
extern AsyncWebServer server;

void setupWiFi();
void initWebServer();
void handleRoot(AsyncWebServerRequest *request);
#endif
/*
 *                                                         *** TIJDELIJK ***   
 *  Web Configuratie voor de Kado-Klok
 *  Hierin definieer ik alle instellingen en functies die nodig zijn voor de WiFi configuratie via een webinterface.
 *  Dit maakt het mogelijk om eenvoudig WiFi gegevens in te voeren zonder de code aan te passen, ideaal voor eindgebruikers.
 *  
 *  - setupWiFi(): Verbindt met het opgeslagen WiFi netwerk of zet een Access Point op als er geen netwerk is.
 *  - initWebServer(): Stelt de webserver in met routes voor het configureren van WiFi gegevens.
 *  - handleRoot(): Handelt de hoofdpagina af waar gebruikers hun WiFi gegevens kunnen invoeren.
 *  
 *  Deze aanpak zorgt voor een gebruiksvriendelijke setup ervaring en maakt het apparaat toegankelijk voor iedereen, ongeacht technische kennis.
 * 
 *                                                        *** TIJDELIJK ***   
 */