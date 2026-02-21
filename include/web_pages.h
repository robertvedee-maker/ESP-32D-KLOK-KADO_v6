#pragma once
#include <Arduino.h>

    const char INDEX_HTML[] PROGMEM = R"rawliteral(
    <!DOCTYPE html>
    <html lang="nl">

    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        %STYLE%
    </head>

    <body>
        <div class="dolphin">🐬</div>
        <div class="card">
            <h2>Instellingen</h2>
            <form action="/save" method="POST">

                <label>WiFi Netwerk</label>
                <input type="text" name="ssid" value="%SSID%" placeholder="WiFi Naam">

                <label>Wachtwoord</label>
                <input type="password" name="pass" value="%PASS%" placeholder="Wachtwoord">

                <label>OpenWeatherMap 3.0 Key</label>
                <input type="text" id="owm_key" name="owm_key" value="%OWM_KEY%" placeholder="32-tekens API Key">

                <label>Locatie Zoeken (Postcode)</label>
                <div style="display: flex; gap: 10px; margin-bottom: 5px;">
                    <input type="text" id="pc" placeholder="1234 AB" style="flex: 2; margin-bottom:0;">
                    <select id="cc" style="flex: 1; margin-bottom:0;">
                        <option value="nl">NL</option>
                        <option value="be">BE</option>
                        <option value="de">DE</option>
                    </select>
                    <button type="button" class="btn" onclick="getGeo()"
                        style="margin:0; width:auto; padding:10px 15px;">🔍</button>
                </div>
                <p id="loc-status" style="font-size:0.75em; color:#888; margin: 0 0 15px 0;">Huidig opgeslagen: %LAT%,
                    %LON%</p>

                <!-- Verborgen velden voor de ESP32 -->
                <input type="hidden" name="lat" id="lat" value="%LAT%">
                <input type="hidden" name="lon" id="lon" value="%LON%">

                <label>Helderheid Dag</label>
                <input type="range" name="d_br" min="0" max="255" value="%D_BR%">

                <label>Transitie Snelheid</label>
                <select name="tr_speed">%SPEED_OPTS%</select>

                <div class="form-group" style="margin-top: 25px; border-top: 1px solid #333; padding-top: 20px;">
                    <span class="switch-label">WiFi Altijd Aan (Updates)</span>
                    <label class="switch">
                        <input type="checkbox" name="wifi_en" %WIFI_CHECKED%>
                        <span class="slider-round"></span>
                    </label>
                    <p style="font-size:0.7em; color:#888; margin-top:8px;">Zet uit voor een stralingsvrije slaapkamer.
                        De klok werkt dan op de laatste cache.</p>
                </div>

                <button type="submit" class="btn">INSTELLINGEN OPSLAAN</button>
            </form>
        </div>

<script>
function getGeo() {
    try {
        var pc = document.getElementById('pc').value.replace(/\s+/g, '');
        var cc = document.getElementById('cc').value;
        var key = document.getElementById('owm_key').value;
        var status = document.getElementById('loc-status');

        if (!pc || !key) {
            alert("Vul a.u.b. de API Key en Postcode in.");
            return;
        }

        status.innerHTML = "⌛ Bezig met aanvraag...";

        var url = "https://api.openweathermap.org/geo/1.0/zip?zip=" + pc + "," + cc + "&appid=" + key;

        // Dit opent de link in een nieuw venster als de fetch blokkeert
        // Zo kan de ontvanger de getallen zelf zien als de automatisering faalt
        fetch(url)
            .then(function(response) {
                if (!response.ok) throw new Error('API-fout');
                return response.json();
            })
            .then(function(data) {
                document.getElementById('lat').value = data.lat;
                document.getElementById('lon').value = data.lon;
                status.innerHTML = "✅ Gevonden: " + data.name + " (" + data.lat + ")";
                status.style.color = "#28a745";
            })
            .catch(function(err) {
                status.innerHTML = "❌ Fout! Klik <a href='" + url + "' target='_blank'>HIER</a> om handmatig te kijken.";
                status.style.color = "#dc3545";
            });
    } catch (e) {
        alert("Script fout: " + e.message);
    }
}
</script>

    </body>

    </html>
    )rawliteral";


    const char RESTART_HTML[] PROGMEM = R"rawliteral(
    <!DOCTYPE html>
    <html>

    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        %STYLE%
    </head>

    <body>
        <div class="dolphin">🐬</div>
        <div class="card" style="text-align: center;">
            <h2>Instellingen Opgeslagen</h2>
            <p style="color:#888;">De Kado-Klok wordt nu opnieuw geconfigureerd.</p>

            <div class="loader-container">
                <div id="bar" class="loader-bar"></div>
            </div>

            <p id="status-text" style="font-size: 0.8em; color: var(--accent);">Initialiseren...</p>
        </div>

        <script>

            // Start de balk en verander de tekst halverwege
            setTimeout(() => {
                const bar = document.getElementById('bar');
                if (bar) bar.style.width = '100%';
            }, 200);

            setTimeout(() => {
                document.getElementById('status-text').innerHTML = "Bijna klaar...";
            }, 5000);

            // De redirect gebeurt vlak voordat de ESP echt uitvalt
            setTimeout(() => { window.location.href = '/'; }, 9500);
        </script>
    </body>

    </html>
    )rawliteral";