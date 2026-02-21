/*
 * (c)2026 R van Dorland - Licensed under MIT License
 * Network logic: WiFi en OTA setup
 */

#pragma once

#include "network_logic.h"
#include "helpers.h"
#include "secret.h"
#include "bitmaps/BootImages.h"

#include <DNSServer.h>

extern DNSServer dnsServer;

// Belofte dat de functies bestaan (alleen de naam en de ;)
// void toonNetwerkInfo();
// void setupWiFi(const char* ssid, const char* password);
void setupWiFi();
void startSetupMode();
void setupOTA();
void disableWiFi();
void enableWiFi();
void handleServerControl();
void handleWiFiEco();
// void startDNSServer();
// void stopDNSServer();
// void handleDNSServer();
void activateWiFiAndServer();
void deactivateWiFiAndServer();
void manageServerTimeout();