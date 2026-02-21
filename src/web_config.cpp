#include "web_config.h"
#include "global_data.h"
#include <Preferences.h>

AsyncWebServer server(80);

void initWebServer()
{
    // 1. De Hoofdpagina (Uitgebreid formulier)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<style>body{background:#121212;color:#E0E0E0;font-family:sans-serif;text-align:center;}";
        html += ".card{background:#1E1E1E;padding:20px;border-radius:10px;display:inline-block;margin-top:20px;border:1px solid #333;width:85%;max-width:400px;}";
        html += "h1{color:#FFD700;margin-bottom:10px;} h2{color:#FFD700;font-size:1.1em;margin-top:20px;}";
        html += "input, select{width:100%;padding:12px;margin:8px 0;background:#333;border:1px solid #555;color:white;box-sizing:border-box;border-radius:5px;}";
        html += "button{width:100%;padding:14px;background:#FFD700;border:none;color:black;font-weight:bold;cursor:pointer;border-radius:5px;margin-top:20px;}</style></head>";
        html += "<body><div class='card'><h1>Kado-Klok Setup</h1>";
        html += "<form action='/save' method='POST'>";
        
        html += "<h2>WiFi Instellingen</h2>";

        html += "Klok Naam (mDNS):<br><input type='text' name='mdns' value='" + state.network.mdns + "' placeholder='Bijv. kado-klok'>";
        html += "<p style='font-size:0.8em;color:#888;'>Bereikbaar via: http://" + state.network.mdns + ".local</p>";

        html += "SSID:<br><input type='text' name='ssid' value='" + state.network.ssid + "' placeholder='Netwerknaam' required>";
        html += "Wachtwoord:<br><input type='pass' name='pass' value='" + state.network.pass + "' placeholder='Wachtwoord'>";

        html += "WiFi Modus:<br><select name='wmode'>";
        html += "<option value='0'" + String(state.network.wifi_mode == 0 ? " selected" : "") + ">Altijd aan (niet aanbevolen)</option>";
        html += "<option value='1'" + String(state.network.wifi_mode == 1 ? " selected" : "") + ">Alleen bij updates (Standaard)</option>";
        html += "<option value='2'" + String(state.network.wifi_mode == 2 ? " selected" : "") + ">Eco Modus (Uit tussen 23-07u)</option>";
        html += "</select>";

        html += "<h2>Persoonlijke Gegevens</h2>";
        html += "Naam:<br><input type='text' name='uname' value='" + state.user.name + "'>";
        html += "Geboortedatum:<br><input type='date' name='udob' value='" + state.user.dob + "'>";
        
        html += "<button type='submit'>OPSLAAN & HERSTARTEN</button>";
        html += "</form></div></body></html>";

        // Extra kaartje voor de harde reset (buiten het formulier)
        html += "<div style='margin-top:40px; border-top:1px solid #444; padding-top:20px;'>";
        html += "<p style='color:#FF5555;'>Pas op: Hiermee wis je alle instellingen!</p>";
        html += "<button onclick=\"if(confirm('Weet je zeker dat je alle instellingen wilt wissen?')) window.location.href='/reset'\" style='background:#AA0000;color:white;'>FABRIEKSINSTELLINGEN</button>";
        html += "</div></div></body></html>";


    request->send(200, "text/html", html); });

        // 2. De Verwerkingspagina (Save & Reboot)
    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
        // --- STAP 0: DATA UIT HET FORMULIER HALEN ---
        // We kijken of de parameters in het POST request zitten, zo niet dan gebruiken we een fallback
        String ssidInput  = request->hasParam("ssid", true)  ? request->getParam("ssid", true)->value()  : "";
        String passInput  = request->hasParam("pass", true)  ? request->getParam("pass", true)->value()  : "";
        String wmodeInput = request->hasParam("wmode", true) ? request->getParam("wmode", true)->value() : "0";
        String unameInput = request->hasParam("uname", true) ? request->getParam("uname", true)->value() : "";
        String udobInput  = request->hasParam("udob", true)  ? request->getParam("udob", true)->value()  : "";
        String rawMdns    = request->hasParam("mdns", true)  ? request->getParam("mdns", true)->value()  : "klok-kado";
        
        // --- STAP 1: mDNS Opschonen ---
        rawMdns.toLowerCase();
        String cleanMdns = "";
        for (int i = 0; i < rawMdns.length(); i++) {
            char c = rawMdns[i];
            if (isAlphaNumeric(c) || c == '-') {
                cleanMdns += c;
            } else if (isSpace(c)) {
                cleanMdns += '-';
            }
        }
        if (cleanMdns == "") cleanMdns = "klok-kado";

        // --- STAP 2: DE STATE UPDATEN (RAM) ---
        // Nu stoppen we de vers binnengekomen data in onze globale variabelen
        state.network.mdns = cleanMdns;
        state.network.ssid = ssidInput;
        state.network.pass = passInput;
        state.network.wifi_mode = wmodeInput.toInt();
        state.user.name = unameInput;
        state.user.dob  = udobInput;

        // --- STAP 3: OPSLAAN IN FLASH (Preferences) ---
        Preferences prefs;
        prefs.begin("clock_cfg", false);
        prefs.putString("mdns",      state.network.mdns);
        prefs.putString("ssid",      state.network.ssid);
        prefs.putString("pass",      state.network.pass);
        prefs.putInt("wifi_mode",    state.network.wifi_mode);
        prefs.putString("user_name", state.user.name);
        prefs.putString("user_dob",  state.user.dob);
        prefs.end();

        // --- STAP 4: De Redirect via IP-Adres ---
        String currentIP = WiFi.localIP().toString();
        if (currentIP == "0.0.0.0") currentIP = "192.168.4.1"; 

        String html = "<html><head>";
        html += "<meta http-equiv='refresh' content='10;url=http://" + currentIP + "/'>";
        html += "<script>window.history.replaceState({}, '', '/');</script>";
        html += "<style>body{background:#121212;color:white;text-align:center;padding-top:50px;}</style></head>";
        html += "<body><h1>Opgeslagen!</h1>";
        html += "<p>De Kado-Klok verbindt nu met: <b>" + state.network.ssid + "</b></p>";
        html += "<p>Naam: <b>" + state.network.mdns + ".local</b></p>";
        html += "<p>Je wordt over 10 seconden teruggebracht...</p></body></html>";

        request->send(200, "text/html", html);

        // --- STAP 5: Debug naar S-Mon ---
        Serial.println(F("--- GEGEVENS ONTVANGEN EN OPGESLAGEN ---"));
        Serial.print(F("SSID: ")); Serial.println(state.network.ssid);
        Serial.print(F("mDNS: ")); Serial.println(state.network.mdns);

        state.network.pending_restart = true;
        state.network.restart_at = millis() + 2000;
    });

    // 3. Reset Endpoint (Fabrieksinstellingen)
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
              {

        // 2. Wis de fysieke opslag (Flash)
        Preferences prefs;
        
        // Gebruik hier de namespace die je ook in initStorage() gebruikt!
        prefs.begin("clock_cfg", false); 
        prefs.clear(); 
        prefs.end();

        // 1. Maak de variabelen in het geheugen (RAM) leeg
        state.user.name = "";
        state.user.dob = "";
        state.network.ssid = "";
        state.network.pass = "";
        state.network.wifi_mode = 0;

        // 3. Bevestig naar de gebruiker
        String response = "<html><body style='background:#121212;color:white;text-align:center;font-family:sans-serif;padding-top:50px;'>";
        response += "<h1>Fabrieksinstellingen Hersteld!</h1><p>De Kado-Klok herstart nu in Setup-modus.</p></body></html>";
        request->send(200, "text/html", response);

        // 4. Herstart
        Serial.println(F("[RESET] Geheugen gewist, herstarten..."));
        delay(2000);
        ESP.restart(); });

    // Trucje voor Android (Google)
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(204); });

    // Trucje voor Apple (iOS)
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); });

    // Catch-all voor andere portal-checks
    server.on("/check_network_status", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200); });

    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(200, "text/html", "<html><head><script>window.location.href='/';</script></head></html>"); });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server.begin();
    Serial.println(F("Webserver: Actief en luisterend..."));
}