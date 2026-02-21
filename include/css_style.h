

#pragma once
#include <Arduino.h>

const char CSS_MAIN[] PROGMEM = R"rawliteral(
<style>
    :root {
        --bg: #0f0f12;
        --card: #1a1a1f;
        --accent: #00d2ff;
        --text: #e0e0e0;
        --input: #25252b;
    }
    body { 
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
        background-color: var(--bg); 
        color: var(--text); 
        margin: 0; padding: 5px;
        display: flex; 
        flex-direction: column; 
        align-items: center; 
        min-height: 100vh;
    }
    .dolphin { font-size: 3rem; margin-top: 20px; filter: drop-shadow(0 0 10px var(--accent)); }
    .card { 
        background: var(--card); 
        padding: 25px; 
        border-radius: 15px; 
        box-shadow: 0 10px 30px rgba(0,0,0,0.5); 
        width: 90%; 
        max-width: 400px; 
        margin: 20px 0;
    }
    h2 { color: var(--accent); margin-top: 0; text-align: center; letter-spacing: 1px; }
    label { display: block; margin-bottom: 5px; font-size: 0.9em; font-weight: bold; color: #888; }
    input[type="text"], input[type="password"], select { 
        width: 100%; padding: 12px; margin-bottom: 18px; border: none; border-radius: 8px; 
        background: var(--input); color: white; box-sizing: border-box; font-size: 16px;
    }
    input[type="range"] { width: 100%; margin: 15px 0; accent-color: var(--accent); }
    .btn { 
        background: linear-gradient(135deg, #00d2ff 0%, #3a7bd5 100%); 
        color: white; border: none; padding: 15px; width: 100%; border-radius: 8px; 
        font-weight: bold; cursor: pointer; transition: transform 0.2s, box-shadow 0.2s; 
    }
    .btn:active { transform: scale(0.98); }
    
    /* Switch styling */
    .switch-label { font-weight: bold; font-size: 0.9em; }
    .switch { position: relative; display: inline-block; width: 50px; height: 26px; float: right; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider-round { 
        position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; 
        background-color: #333; transition: .4s; border-radius: 34px; 
    }
    .slider-round:before { 
        position: absolute; content: ""; height: 18px; width: 18px; left: 4px; bottom: 4px; 
        background-color: white; transition: .4s; border-radius: 50%; 
    }
    input:checked + .slider-round { background-color: var(--accent); }
    input:checked + .slider-round:before { transform: translateX(24px); }

    /* Loader voor herstart pagina */
    .loader-container { width: 100%; background: #333; height: 10px; border-radius: 5px; margin-top: 20px; overflow: hidden; }
    .loader-bar { width: 0%; height: 100%; background: var(--accent); transition: width 9s linear; }
</style>
)rawliteral";
