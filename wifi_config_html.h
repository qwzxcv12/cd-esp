#ifndef WIFI_CONFIG_HTML_H
#define WIFI_CONFIG_HTML_H

const char* html_page = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Device Setup</title>
    <style>
        :root {
            --ink: #0b0f14;
            --panel: #11161d;
            --line: #232b35;
            --text: #e6edf3;
            --muted: #6b7785;
            --accent: #ffb454;
            --accent-dim: rgba(255, 180, 84, 0.16);
            --ok: #5ec98f;
            --mono: ui-monospace, 'SF Mono', 'Cascadia Code', 'Consolas', 'Courier New', monospace;
            --sans: -apple-system, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
        }
        * { box-sizing: border-box; }
        body {
            font-family: var(--sans);
            background-color: var(--ink);
            background-image:
                linear-gradient(90deg, rgba(255, 180, 84, 0.05) 1px, transparent 1px),
                linear-gradient(rgba(255, 180, 84, 0.05) 1px, transparent 1px),
                radial-gradient(circle, rgba(255, 180, 84, 0.14) 1.4px, transparent 1.4px);
            background-size: 56px 56px, 56px 56px, 56px 56px;
            background-position: 0 0, 0 0, 28px 28px;
            color: var(--text);
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            margin: 0;
            padding: 24px 0;
        }
        .panel {
            width: 400px;
            max-width: 92%;
            background: var(--panel);
            border: 1px solid var(--line);
            border-top: 2px solid var(--accent);
            border-radius: 8px;
            box-shadow: 0 20px 50px rgba(0, 0, 0, 0.45);
            overflow: hidden;
        }
        .panel__header {
            display: flex;
            align-items: flex-start;
            gap: 12px;
            padding: 22px 24px 18px;
            border-bottom: 1px solid var(--line);
        }
        .chip-icon {
            flex: none;
            width: 30px;
            height: 30px;
            color: var(--accent);
            margin-top: 2px;
        }
        .panel__heading {
            flex: 1;
            min-width: 0;
        }
        .device-id {
            font-family: var(--mono);
            font-size: 11px;
            letter-spacing: 1.5px;
            color: var(--muted);
            text-transform: uppercase;
        }
        h2 {
            margin: 2px 0 6px;
            font-size: 19px;
            font-weight: 600;
            color: var(--text);
        }
        .subtitle {
            font-size: 12.5px;
            line-height: 1.5;
            color: var(--muted);
            margin: 0;
        }
        .status {
            flex: none;
            display: flex;
            align-items: center;
            gap: 6px;
            font-family: var(--mono);
            font-size: 10.5px;
            letter-spacing: 1px;
            color: var(--ok);
            white-space: nowrap;
            padding-top: 3px;
        }
        .status-dot {
            width: 6px;
            height: 6px;
            border-radius: 50%;
            background: var(--ok);
            box-shadow: 0 0 6px rgba(94, 201, 143, 0.8);
            animation: pulse 2s ease-in-out infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.35; }
        }
        @media (prefers-reduced-motion: reduce) {
            .status-dot { animation: none; }
        }
        form {
            padding: 4px 24px 24px;
        }
        .section {
            padding-top: 20px;
        }
        .section__title {
            display: flex;
            align-items: baseline;
            gap: 8px;
            margin-bottom: 14px;
        }
        .section__index {
            font-family: var(--mono);
            font-size: 11px;
            color: var(--accent);
        }
        .section__label {
            flex: none;
            font-family: var(--mono);
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 1.5px;
            text-transform: uppercase;
            color: var(--text);
        }
        .section__title::after {
            content: '';
            flex: 1;
            height: 1px;
            background: var(--line);
        }
        .field {
            margin-bottom: 12px;
        }
        .field label {
            display: block;
            margin-bottom: 5px;
            font-family: var(--mono);
            font-size: 11px;
            letter-spacing: 0.5px;
            color: var(--muted);
        }
        input[type="text"],
        input[type="password"],
        input[type="number"] {
            width: 100%;
            padding: 10px 12px;
            font-family: var(--mono);
            font-size: 13.5px;
            color: var(--text);
            background: rgba(0, 0, 0, 0.35);
            border: 1px solid var(--line);
            border-radius: 5px;
            outline: none;
            transition: border-color 0.15s ease, box-shadow 0.15s ease;
        }
        input::placeholder {
            color: #45505c;
        }
        input:focus {
            border-color: var(--accent);
            box-shadow: 0 0 0 3px var(--accent-dim);
        }
        .submit {
            width: 100%;
            margin-top: 22px;
            padding: 13px;
            font-family: var(--mono);
            font-size: 13px;
            font-weight: 700;
            letter-spacing: 1.5px;
            text-transform: uppercase;
            color: var(--ink);
            background: var(--accent);
            border: none;
            border-radius: 6px;
            cursor: pointer;
            transition: background 0.15s ease, transform 0.05s ease;
        }
        .submit:hover { background: #ffc578; }
        .submit:active { transform: translateY(1px); }
        .submit:focus-visible {
            outline: 2px solid var(--accent);
            outline-offset: 3px;
        }
        .footnote {
            margin-top: 14px;
            text-align: center;
            font-family: var(--mono);
            font-size: 10.5px;
            color: var(--muted);
            line-height: 1.6;
        }
        .footnote strong {
            color: #8a96a3;
            font-weight: 600;
        }
    </style>
</head>
<body>
    <div class="panel">
        <div class="panel__header">
            <svg class="chip-icon" viewBox="0 0 32 32" fill="none" xmlns="http://www.w3.org/2000/svg">
                <rect x="9" y="9" width="14" height="14" rx="1.5" stroke="currentColor" stroke-width="1.6"/>
                <rect x="13" y="13" width="6" height="6" rx="0.5" stroke="currentColor" stroke-width="1.4"/>
                <path d="M9 13H4M9 19H4M28 13H23M28 19H23M13 9V4M19 9V4M13 28V23M19 28V23" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/>
            </svg>
            <div class="panel__heading">
                <div class="device-id">ESP32 &middot; SoC</div>
                <h2>Device Configuration</h2>
                <p class="subtitle">Set the network, broker, and socket endpoint this unit will use after it reboots.</p>
            </div>
            <div class="status"><span class="status-dot"></span>AP MODE</div>
        </div>

        <form action="/config" method="POST">
            <div class="section">
                <div class="section__title">
                    <span class="section__index">01</span>
                    <span class="section__label">Network</span>
                </div>
                <div class="field">
                    <label for="ssid">Wi-Fi network name (SSID)</label>
                    <input type="text" id="ssid" name="ssid" required placeholder="Enter Wi-Fi SSID" value="{{SSID}}">
                </div>
                <div class="field">
                    <label for="password">Wi-Fi password</label>
                    <input type="password" id="password" name="password" placeholder="Enter Wi-Fi password" value="{{PASSWORD}}">
                </div>
            </div>

            <div class="section">
                <div class="section__title">
                    <span class="section__index">02</span>
                    <span class="section__label">MQTT Broker</span>
                </div>
                <div class="field">
                    <label for="mqtt_server">Broker host</label>
                    <input type="text" id="mqtt_server" name="mqtt_server" placeholder="e.g. broker.hivemq.com" value="{{MQTT_SERVER}}">
                </div>
                <div class="field">
                    <label for="mqtt_port">Broker port</label>
                    <input type="number" id="mqtt_port" name="mqtt_port" placeholder="1883" value="{{MQTT_PORT}}">
                </div>
                <div class="field">
                    <label for="mqtt_user">Broker username</label>
                    <input type="text" id="mqtt_user" name="mqtt_user" placeholder="Optional" value="{{MQTT_USER}}">
                </div>
                <div class="field">
                    <label for="mqtt_pass">Broker password</label>
                    <input type="password" id="mqtt_pass" name="mqtt_pass" placeholder="Optional" value="{{MQTT_PASS}}">
                </div>
                <div class="field">
                    <label for="mqtt_topic">Topic</label>
                    <input type="text" id="mqtt_topic" name="mqtt_topic" placeholder="e.g. esp32/sensor" value="{{MQTT_TOPIC}}">
                </div>
            </div>

            <div class="section">
                <div class="section__title">
                    <span class="section__index">03</span>
                    <span class="section__label">WebSocket</span>
                </div>
                <div class="field">
                    <label for="ws_url">Socket URL</label>
                    <input type="text" id="ws_url" name="ws_url" placeholder="e.g. ws://192.168.1.100:81" value="{{WS_URL}}">
                </div>
            </div>

            <button type="submit" class="submit">Save &amp; Reboot</button>
            <div class="footnote">Settings are written to flash and applied on reboot.<br><strong>Antigravity ESP-IDF Manager</strong></div>
        </form>
    </div>
</body>
</html>
)html";

#endif // WIFI_CONFIG_HTML_H
