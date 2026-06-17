#ifndef WIFI_CONFIG_HTML_H
#define WIFI_CONFIG_HTML_H

const char* html_page = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 WiFi Configuration</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e1e2f 0%, #111119 100%);
            color: #f0f0f0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
        }
        .container {
            background: rgba(255, 255, 255, 0.05);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            padding: 30px;
            border-radius: 15px;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37);
            width: 320px;
            text-align: center;
        }
        h2 {
            margin-bottom: 20px;
            color: #00adb5;
        }
        .input-group {
            margin-bottom: 15px;
            text-align: left;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-size: 14px;
            color: #aaaaaa;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 10px;
            box-sizing: border-box;
            border: 1px solid rgba(255, 255, 255, 0.2);
            background: rgba(0, 0, 0, 0.2);
            color: #ffffff;
            border-radius: 5px;
            outline: none;
            transition: 0.3s;
        }
        input:focus {
            border-color: #00adb5;
            box-shadow: 0 0 8px rgba(0, 173, 181, 0.5);
        }
        button {
            width: 100%;
            padding: 12px;
            background: #00adb5;
            border: none;
            color: white;
            font-size: 16px;
            font-weight: bold;
            border-radius: 5px;
            cursor: pointer;
            transition: 0.3s;
            margin-top: 10px;
        }
        button:hover {
            background: #00bfa5;
            box-shadow: 0 0 15px rgba(0, 191, 165, 0.6);
        }
        .footer {
            margin-top: 20px;
            font-size: 11px;
            color: #666666;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>ESP32 WiFi Config</h2>
        <form action="/config" method="POST">
            <div class="input-group">
                <label for="ssid">WiFi SSID</label>
                <input type="text" id="ssid" name="ssid" required placeholder="Enter SSID">
            </div>
            <div class="input-group">
                <label for="password">Password</label>
                <input type="password" id="password" name="password" placeholder="Enter Password">
            </div>
            <button type="submit">Save & Connect</button>
        </form>
        <div class="footer">Antigravity ESP-IDF WiFi Manager</div>
    </div>
</body>
</html>
)html";

#endif // WIFI_CONFIG_HTML_H
