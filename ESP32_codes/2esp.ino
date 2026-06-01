/*
   ================================================================================
   ESP32 ROVER - SYSTEM 2 (SD CARD DATA LOGGER & TELEGRAM BOT VAULT)
   ================================================================================
   
   GLOBAL ROVER SYSTEM FEATURE MAP:
   
   1. ESP1 (Control, Siren & Robotic Arm) Features:
      1.1. BTS7960 Motor Driver Control (Forward, Backward, Left, Right, Speed)
      1.2. Alert Buzzer/Siren System (Gas alarm, Human beep, Fire, Low battery, SOS, Obstacle)
      1.3. Integrated Web UI for manual driving & manual alarm triggers
      1.4. Non-Blocking MQTT Command Listener (controls motor, plays alarm tones, controls robotic arm & pan-tilt servos)
      1.5. 2-Axis Pan-Tilt Servo Controller (For camera/scanner alignment)
      1.6. Front & Back Obstacle Avoidance (Ultrasonic safety auto-reverse and auto-advance overrides)
      1.7. 4-Axis Robotic Arm Control (Base, Shoulder, Elbow, Gripper servos on ESP1)
      
   2. ESP2 (SD Card Log Vault & Telegram Bot) Features:
      2.1. VSPI-based SD Card Data Logging (CSV format with auto-headers)
      2.2. Web-based File Manager UI (View files, Download logs, Delete files)
      2.3. Live Diagnostics Dashboard (AJAX updates for temperature, humidity, GPS, fire status)
      2.4. Non-Blocking MQTT Logger (receives and logs all node telemetry)
      2.5. Telegram Bot Interface (Direct system commands, manual checks, live diagnostics)
      2.6. Periodic 10-Second Location Tracker (Sends actual GPS location to Telegram)
      
   3. ESP3 (Sensors & Firefighter) Features:
      3.1. Flame Sensor Alarm System (3x flame detectors triggering spray pump relay)
      3.2. Sweeping Servo Nozzle (automatic 0-180 firefighting sweeps)
      3.3. NEO-6M GPS Tracker (TinyGPS++ live positioning and status coordinates)
      3.4. Weather Monitor (DHT11 Temperature & Humidity tracker)
      3.5. Smart Gas Monitor (MQ2, MQ9, MQ135 for Smoke, CO, Flammable Gas, Air Quality)
      3.6. Non-Blocking MQTT Publisher (broadcasts environmental, location, gas, and fire status)
      3.7. Inter-Node Alert link (signals ESP1's siren to play alarm automatically upon fire detection)

   --------------------------------------------------------------------------------
   DEVICE CONFIGURATION (ESP2 SPECIFICS):
   - PIN CONNECTIONS (Standard ESP32 SPI VSPI):
     - MOSI -> GPIO 23
     - MISO -> GPIO 19
     - SCK  -> GPIO 18
     - CS   -> GPIO 5  (SD Chip Select)

   MQTT TOPICS:
   - Subscribed Topics:
     - "rover/sensors/dht"    (Payload: {"temp":XX.X,"hum":YY.Y})
     - "rover/sensors/gps"    (Payload: {"lat":XX.XXXX,"lon":YY.YYYY,"speed":S.S,"sat":N,"valid":bool})
     - "rover/sensors/fire"   (Payload: ON / OFF)
     - "rover/control/status" (Payload: MOTOR:XXXX | SPEED:YYY)
   - Published Topics:
     - "rover/control/cmd"    (For stopping/adjusting rover via Telegram)
     - "rover/siren/cmd"      (For sounding alerts via Telegram)

   ================================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// ==========================================
// WIFI & MQTT CONFIGURATION
// ==========================================
const char* ssid          = "sim";
const char* password      = "simple12";
const char* mqtt_server   = "broker.hivemq.com"; // Public Broker from your settings
const int mqtt_port       = 1883;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(80);

// ==========================================
// TELEGRAM CONFIGURATION
// ==========================================
// Enter your details below after setting up with BotFather
const char* botToken = "8343462985:AAH9UoSoi90KGlqNQV06ZG75cc704_NXdxY";
const char* chatID   = "7901238515";

WiFiClientSecure securedClient;
UniversalTelegramBot bot(botToken, securedClient);

bool liveLocationActive = false;
unsigned long lastTelegramPoll = 0;
unsigned long lastLiveLocationTime = 0;

// ==========================================
// CLOUD BLACKBOX CONFIGURATION (Firebase)
// ==========================================
// Paste your Firebase Realtime Database URL here (e.g. "https://my-rover-rtdb.firebaseio.com/")
// Note: Keep the trailing slash. Set to "" to disable.
const char* firebaseHost = "https://disasterrover-c6836-default-rtdb.firebaseio.com/";

// ==========================================
// LIVE SENSOR VARIABLES
// ==========================================
float temp = 0.0;
float hum = 0.0;
double gps_lat = 22.989106;
double gps_lon = 88.453064;
double gps_speed = 0.0;
int gps_sat = 0;
bool gps_valid = false;
String fire_status = "OFF";
String last_motor_cmd = "UNKNOWN";

// MQ Gas Sensors Telemetry
float mq2_val = 0.0;
float mq9_val = 0.0;
float mq135_val = 0.0;
String gas_overall_status = "ENVIRONMENT NORMAL";

// System Danger State
bool isDanger = false;
bool lastDangerState = false;
unsigned long lastTelegramAlertTime = 0;

bool sdMounted = false;

// ==========================================
// WEB SERVER HTML PAGE (Slate/Amber Theme)
// ==========================================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Rover Telemetry Vault</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #080c14;
            --card-bg: rgba(15, 23, 42, 0.75);
            --accent-amber: #f59e0b;
            --accent-cyan: #06b6d4;
            --accent-red: #ef4444;
            --accent-green: #10b981;
            --text-color: #f3f4f6;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            background-color: var(--bg-color);
            background-image: 
                radial-gradient(at 0% 0%, rgba(245, 158, 11, 0.1) 0px, transparent 40%),
                radial-gradient(at 100% 100%, rgba(6, 182, 212, 0.1) 0px, transparent 40%);
            color: var(--text-color);
            font-family: 'Outfit', sans-serif;
            min-height: 100vh;
            padding: 30px 15px;
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        header {
            text-align: center;
            margin-bottom: 30px;
        }
        header h1 {
            font-size: 2.3rem;
            font-weight: 700;
            background: linear-gradient(135deg, var(--accent-amber), var(--accent-cyan));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-transform: uppercase;
            letter-spacing: 2px;
        }
        header p {
            font-size: 0.95rem;
            color: #9ca3af;
            margin-top: 5px;
            font-weight: 300;
        }
        .container {
            width: 100%;
            max-width: 1100px;
            display: grid;
            grid-template-columns: 1fr;
            gap: 25px;
        }
        .card {
            background: var(--card-bg);
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 20px;
            padding: 25px;
            backdrop-filter: blur(12px);
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.4);
        }
        .card-title {
            font-size: 1.25rem;
            font-weight: 600;
            margin-bottom: 20px;
            color: var(--accent-amber);
            border-bottom: 1px solid rgba(255, 255, 255, 0.08);
            padding-bottom: 10px;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        /* Live Stats Grid */
        .telemetry-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .telemetry-item {
            background: rgba(30, 41, 59, 0.5);
            border: 1px solid rgba(255, 255, 255, 0.03);
            border-radius: 14px;
            padding: 18px;
            display: flex;
            flex-direction: column;
            gap: 5px;
            transition: all 0.2s ease;
        }
        .telemetry-item:hover {
            border-color: rgba(245, 158, 11, 0.2);
            background: rgba(30, 41, 59, 0.7);
        }
        .telemetry-label {
            font-size: 0.85rem;
            color: #9ca3af;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .telemetry-value {
            font-size: 1.5rem;
            font-weight: 700;
            color: var(--text-color);
        }
        .telemetry-value.active { color: var(--accent-cyan); }
        .telemetry-value.danger { color: var(--accent-red); animation: pulse 1.5s infinite; }
        .telemetry-value.ok { color: var(--accent-green); }
        
        @keyframes pulse {
            0% { opacity: 0.7; }
            50% { opacity: 1; text-shadow: 0 0 10px rgba(239, 68, 68, 0.5); }
            100% { opacity: 0.7; }
        }

        /* File Explorer UI */
        .file-section {
            overflow-x: auto;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            text-align: left;
            margin-top: 10px;
        }
        th, td {
            padding: 14px 16px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
        }
        th {
            font-weight: 600;
            color: #9ca3af;
            font-size: 0.9rem;
            text-transform: uppercase;
        }
        tr:hover td {
            background: rgba(255, 255, 255, 0.02);
        }
        td {
            font-size: 0.95rem;
        }
        .btn {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            padding: 8px 14px;
            border-radius: 8px;
            font-size: 0.85rem;
            font-weight: 600;
            text-decoration: none;
            transition: all 0.2s;
            margin-right: 8px;
            border: none;
            cursor: pointer;
        }
        .btn-view {
            background: rgba(6, 182, 212, 0.15);
            color: var(--accent-cyan);
        }
        .btn-view:hover {
            background: var(--accent-cyan);
            color: #080c14;
        }
        .btn-download {
            background: rgba(245, 158, 11, 0.15);
            color: var(--accent-amber);
        }
        .btn-download:hover {
            background: var(--accent-amber);
            color: #080c14;
        }
        .btn-delete {
            background: rgba(239, 68, 68, 0.15);
            color: var(--accent-red);
        }
        .btn-delete:hover {
            background: var(--accent-red);
            color: white;
        }
        .status-bar {
            width: 100%;
            display: flex;
            justify-content: space-between;
            align-items: center;
            background: rgba(15, 23, 42, 0.5);
            padding: 12px 20px;
            border-radius: 10px;
            font-size: 0.85rem;
            margin-top: 20px;
            border: 1px solid rgba(255, 255, 255, 0.03);
            color: #9ca3af;
        }
        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            display: inline-block;
            margin-right: 6px;
        }
        .status-dot.on { background: var(--accent-green); box-shadow: 0 0 8px var(--accent-green); }
        .status-dot.off { background: var(--accent-red); box-shadow: 0 0 8px var(--accent-red); }
    </style>
</head>
<body>
    <header>
        <h1>📁 Telemetry Log Vault</h1>
        <p>ESP32 SD Card Data Logger & File Manager</p>
    </header>

    <div class="container">
        <!-- Live Telemetry Card -->
        <div class="card">
            <div class="card-title">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
                Real-Time Telemetry Feed
            </div>
            <div class="telemetry-grid">
                <div class="telemetry-item">
                    <span class="telemetry-label">DHT11 Temp</span>
                    <span class="telemetry-value active" id="valTemp">0.0 °C</span>
                </div>
                <div class="telemetry-item">
                    <span class="telemetry-label">DHT11 Hum</span>
                    <span class="telemetry-value active" id="valHum">0.0 %</span>
                </div>
                <div class="telemetry-item">
                    <span class="telemetry-label">GPS Location</span>
                    <span class="telemetry-value" id="valGps" style="font-size:1.15rem; font-weight:600; padding-top:6px;">No Fix</span>
                </div>
                <div class="telemetry-item">
                    <span class="telemetry-label">Fire Alert Status</span>
                    <span class="telemetry-value" id="valFire">SAFE</span>
                </div>
                <div class="telemetry-item">
                    <span class="telemetry-label">Rover Engine Action</span>
                    <span class="telemetry-value active" id="valMotor" style="font-size:1.1rem; font-weight:600; padding-top:8px;">UNKNOWN</span>
                </div>
            </div>
        </div>

        <!-- SD Card Files Card -->
        <div class="card">
            <div class="card-title">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
                SD Storage Manager
            </div>
            <div class="file-section">
                <table>
                    <thead>
                        <tr>
                            <th>File Name</th>
                            <th>Size</th>
                            <th>Actions</th>
                        </tr>
                    </thead>
                    <tbody id="fileListBody">
                        %FILE_LIST%
                    </tbody>
                </table>
            </div>
        </div>
        
        <!-- Status Bar -->
        <div class="status-bar">
            <span>SD Card State: <span id="sdStatus">MOUNTED</span></span>
            <span>MQTT Link: <span id="mqttStatus">CONNECTING...</span></span>
        </div>
    </div>

    <script>
        function updateLiveTelemetry() {
            fetch('/api/telemetry')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('valTemp').innerText = data.temp.toFixed(1) + ' °C';
                    document.getElementById('valHum').innerText = data.hum.toFixed(1) + ' %';
                    
                    if (data.gpsValid) {
                        document.getElementById('valGps').innerHTML = `L: ${data.lat.toFixed(4)}<br>L: ${data.lon.toFixed(4)}<br><span style="font-size:0.8rem; color:#9ca3af;">Satellites: ${data.sat} | Speed: ${data.speed.toFixed(1)} km/h</span>`;
                    } else {
                        document.getElementById('valGps').innerHTML = '<span style="color:#ef4444;">No GPS Fix</span>';
                    }
                    
                    const fireEl = document.getElementById('valFire');
                    if (data.fire === "ON") {
                        fireEl.innerText = "🔥 ALARM ON";
                        fireEl.className = "telemetry-value danger";
                    } else {
                        fireEl.innerText = "SAFE";
                        fireEl.className = "telemetry-value ok";
                    }
                    
                    document.getElementById('valMotor').innerText = data.motor;
                    
                    const sdStatusEl = document.getElementById('sdStatus');
                    sdStatusEl.innerHTML = data.sdMounted ? '<span class="status-dot on"></span>MOUNTED' : '<span class="status-dot off"></span>UNMOUNTED';
                    
                    const mqttStatusEl = document.getElementById('mqttStatus');
                    mqttStatusEl.innerHTML = data.mqtt ? '<span class="status-dot on"></span>CONNECTED' : '<span class="status-dot off"></span>DISCONNECTED';
                });
        }
        
        setInterval(updateLiveTelemetry, 2000);
        updateLiveTelemetry();
    </script>
</body>
</html>
)rawliteral";

// ==========================================
// FILES TABLE GENERATOR
// ==========================================
String getFilesTable() {
  String html = "";
  if (!sdMounted) {
    return "<tr><td colspan='3' style='text-align:center; color:var(--accent-red);'>SD Card Not Mounted!</td></tr>";
  }

  File root = SD.open("/");
  if (!root) {
    return "<tr><td colspan='3' style='text-align:center; color:var(--accent-red);'>Failed to open SD directory!</td></tr>";
  }

  File file = root.openNextFile();
  int count = 0;
  while (file) {
    if (!file.isDirectory()) {
      count++;
      String name = String(file.name());
      String displayName = name;
      if (displayName.startsWith("/")) {
        displayName = displayName.substring(1);
      }
      float sizeKB = file.size() / 1024.0;
      
      html += "<tr>";
      html += "<td>📄 " + displayName + "</td>";
      html += "<td>" + String(sizeKB, 2) + " KB</td>";
      html += "<td>";
      html += "<a class='btn btn-view' href='/view?file=" + name + "' target='_blank'>👁 View</a>";
      html += "<a class='btn btn-download' href='/download?file=" + name + "'>📥 Download</a>";
      html += "<a class='btn btn-delete' href='/delete?file=" + name + "' onclick=\"return confirm('Are you sure you want to delete this file?')\">🗑 Delete</a>";
      html += "</td>";
      html += "</tr>";
    }
    file = root.openNextFile();
  }
  root.close();

  if (count == 0) {
    html = "<tr><td colspan='3' style='text-align:center; color:#9ca3af;'>No files found on SD card.</td></tr>";
  }
  return html;
}

// ==========================================
// MQTT CALLBACK HANDLER & MANUAL JSON PARSING
// ==========================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  String topicStr = String(topic);

  // Parsing temperature and humidity
  if (topicStr == "rover/sensors/dht") {
    int tempIdx = message.indexOf("\"temp\":");
    if (tempIdx != -1) {
      int commaIdx = message.indexOf(",", tempIdx);
      if (commaIdx == -1) commaIdx = message.indexOf("}", tempIdx);
      String tVal = message.substring(tempIdx + 7, commaIdx);
      temp = tVal.toFloat();
    }
    int humIdx = message.indexOf("\"hum\":");
    if (humIdx != -1) {
      int commaIdx = message.indexOf(",", humIdx);
      if (commaIdx == -1) commaIdx = message.indexOf("}", humIdx);
      String hVal = message.substring(humIdx + 6, commaIdx);
      hum = hVal.toFloat();
    }
    Serial.printf("Logged Temp: %.1f C, Hum: %.1f %%\n", temp, hum);
  }
  // Parsing GPS data
  else if (topicStr == "rover/sensors/gps") {
    int latIdx = message.indexOf("\"lat\":");
    if (latIdx != -1) {
      int commaIdx = message.indexOf(",", latIdx);
      gps_lat = message.substring(latIdx + 6, commaIdx).toDouble();
    }
    int lonIdx = message.indexOf("\"lon\":");
    if (lonIdx != -1) {
      int commaIdx = message.indexOf(",", lonIdx);
      gps_lon = message.substring(lonIdx + 6, commaIdx).toDouble();
    }
    int speedIdx = message.indexOf("\"speed\":");
    if (speedIdx != -1) {
      int commaIdx = message.indexOf(",", speedIdx);
      gps_speed = message.substring(speedIdx + 8, commaIdx).toDouble();
    }
    int satIdx = message.indexOf("\"sat\":");
    if (satIdx != -1) {
      int commaIdx = message.indexOf(",", satIdx);
      gps_sat = message.substring(satIdx + 6, commaIdx).toInt();
    }
    int validIdx = message.indexOf("\"valid\":");
    if (validIdx != -1) {
      int closeIdx = message.indexOf("}", validIdx);
      String valStr = message.substring(validIdx + 8, closeIdx);
      gps_valid = (valStr == "true" || valStr == "1");
    }
    Serial.printf("Logged GPS: Lat: %.6f, Lon: %.6f, Valid: %d\n", gps_lat, gps_lon, gps_valid);
  }
  // Parsing Gas data
  else if (topicStr == "rover/sensors/gas") {
    // Parse gasJson manually
    // Expected format: {"mq2":{"val":XX,"stat":"XX"},"mq9":{"val":YY,"stat":"YY"},"mq135":{"val":ZZ,"stat":"ZZ"},"overall":"STATUS"}
    int mq2Idx = message.indexOf("\"mq2\":");
    if (mq2Idx != -1) {
      int valIdx = message.indexOf("\"val\":", mq2Idx);
      if (valIdx != -1) {
        int commaIdx = message.indexOf(",", valIdx);
        if (commaIdx == -1) commaIdx = message.indexOf("}", valIdx);
        mq2_val = message.substring(valIdx + 6, commaIdx).toFloat();
      }
    }
    int mq9Idx = message.indexOf("\"mq9\":");
    if (mq9Idx != -1) {
      int valIdx = message.indexOf("\"val\":", mq9Idx);
      if (valIdx != -1) {
        int commaIdx = message.indexOf(",", valIdx);
        if (commaIdx == -1) commaIdx = message.indexOf("}", valIdx);
        mq9_val = message.substring(valIdx + 6, commaIdx).toFloat();
      }
    }
    int mq135Idx = message.indexOf("\"mq135\":");
    if (mq135Idx != -1) {
      int valIdx = message.indexOf("\"val\":", mq135Idx);
      if (valIdx != -1) {
        int commaIdx = message.indexOf(",", valIdx);
        if (commaIdx == -1) commaIdx = message.indexOf("}", valIdx);
        mq135_val = message.substring(valIdx + 6, commaIdx).toFloat();
      }
    }
    int overallIdx = message.indexOf("\"overall\":\"");
    if (overallIdx != -1) {
      int quoteIdx = message.indexOf("\"", overallIdx + 11);
      gas_overall_status = message.substring(overallIdx + 11, quoteIdx);
    }
    Serial.printf("Logged Gas: MQ2: %.1f, MQ9: %.1f, MQ135: %.1f, Status: %s\n", 
                  mq2_val, mq9_val, mq135_val, gas_overall_status.c_str());
  }
  // Fire Alert trigger logging
  else if (topicStr == "rover/sensors/fire") {
    fire_status = message;
    Serial.println("Logged Fire Status: " + fire_status);
  }
  // Motor operations logging
  else if (topicStr == "rover/control/status") {
    last_motor_cmd = message;
    Serial.println("Logged Engine Command: " + last_motor_cmd);
  }
}

// ==========================================
// TELEGRAM MESSAGE HANDLER
// ==========================================
void handleNewMessages(int numNewMessages) {
  Serial.print("Telegram Bot: handling ");
  Serial.print(numNewMessages);
  Serial.println(" new message(s).");

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    
    // Only accept messages from authorized chat ID
    if (chat_id != String(chatID)) {
      bot.sendMessage(chat_id, "Unauthorized user. Access denied.", "");
      continue;
    }

    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    Serial.println("Received: " + text + " from " + from_name);

    if (text == "/start" || text == "/help") {
      String welcome = "🌌 *Rover GCS Telegram Bot* 🌌\n\n";
      welcome += "Welcome, " + from_name + "! Use the command buttons below to interact with the distributed Rover system.\n\n";
      welcome += "🤖 *Commands:*\n";
      welcome += "📈 /status - View real-time sensor & engine stats\n";
      welcome += "📍 /location - Send live GPS mapping coordinates\n";
      welcome += "🚨 /fire_siren - Sound emergency firefighting sirens\n";
      welcome += "🔇 /mute - Mute ongoing buzzer sirens\n";
      welcome += "⛔ /stop - Emergency halt rover wheel drive motors\n";
      welcome += "⏱ /toggle_live - Toggle 10s automatic live location reporting\n";
      
      // Send message with custom markup keyboard
      String keyboardJson = "[[\"/status\", \"/location\"], [\"/stop\", \"/fire_siren\", \"/mute\"], [\"/toggle_live\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, welcome, "Markdown", keyboardJson, true);
    }
    else if (text == "/status") {
      String stats = "📈 *Rover Telemetry status:*\n\n";
      stats += "🌡 *DHT11 Temperature:* " + String(temp, 1) + " °C\n";
      stats += "💧 *DHT11 Humidity:* " + String(hum, 1) + " %\n";
      stats += "🔥 *Flame Scouting Alert:* " + fire_status + "\n";
      stats += "⚙ *Engine Drive Action:* " + last_motor_cmd + "\n";
      stats += "🛰 *GPS Satellites:* " + String(gps_sat) + "\n";
      stats += "🔗 *MQTT Gateway:* connected\n";
      bot.sendMessage(chat_id, stats, "Markdown");
    }
    else if (text == "/location") {
      if (gps_valid) {
        // Also send Google Maps URL
        String mapLink = "📍 *Live Coordinates:*\n";
        mapLink += "Lat: " + String(gps_lat, 6) + ", Lon: " + String(gps_lon, 6) + "\n";
        mapLink += "Speed: " + String(gps_speed, 1) + " km/h\n";
        mapLink += "[View in Google Maps](https://maps.google.com/?q=" + String(gps_lat, 6) + "," + String(gps_lon, 6) + ")";
        bot.sendMessage(chat_id, mapLink, "Markdown");
      } else {
        String msg = "⚠️ *No Active GNSS fix.*\n";
        msg += "Last Captured Coordinates:\n";
        msg += "Lat: " + String(gps_lat, 6) + ", Lon: " + String(gps_lon, 6) + "\n";
        msg += "[View Last Location](https://maps.google.com/?q=" + String(gps_lat, 6) + "," + String(gps_lon, 6) + ")";
        bot.sendMessage(chat_id, msg, "Markdown");
      }
    }
    else if (text == "/stop") {
      mqttClient.publish("rover/control/cmd", "S");
      bot.sendMessage(chat_id, "⛔ *Emergency Halt:* Stop command published to wheels.", "Markdown");
    }
    else if (text == "/fire_siren") {
      mqttClient.publish("rover/siren/cmd", "fire");
      bot.sendMessage(chat_id, "🚨 *Alert:* Play fire siren command published.", "Markdown");
    }
    else if (text == "/mute") {
      mqttClient.publish("rover/siren/cmd", "stop");
      bot.sendMessage(chat_id, "🔇 *Alert:* Mute alarm command published.", "Markdown");
    }
    else if (text == "/toggle_live") {
      liveLocationActive = !liveLocationActive;
      if (liveLocationActive) {
        bot.sendMessage(chat_id, "⏱ *Live location tracking ENABLED.* You will receive GPS ping reports every 10 seconds.", "Markdown");
      } else {
        bot.sendMessage(chat_id, "⏱ *Live location tracking DISABLED.*", "Markdown");
      }
    }
  }
}

// ==========================================
// TELEGRAM LIVE LOCATION REPETITIVE SENDER
// ==========================================
void checkTelegramLiveLocation() {
  if (liveLocationActive) {
    unsigned long now = millis();
    if (now - lastLiveLocationTime >= 10000 || lastLiveLocationTime == 0) {
      lastLiveLocationTime = now;
      
      Serial.println("Telegram Bot: Sending periodic 10s live location.");
      if (gps_valid) {
        String mapLink = "📍 *Live Coordinates:*\n";
        mapLink += "Lat: " + String(gps_lat, 6) + ", Lon: " + String(gps_lon, 6) + "\n";
        mapLink += "[View in Google Maps](https://maps.google.com/?q=" + String(gps_lat, 6) + "," + String(gps_lon, 6) + ")";
        bot.sendMessage(String(chatID), mapLink, "Markdown");
      } else {
        String msg = "⚠️ *Live Location Alert: No Active Fix.*\n";
        msg += "Last Known Coordinates: " + String(gps_lat, 6) + ", " + String(gps_lon, 6);
        bot.sendMessage(String(chatID), msg, "Markdown");
      }
    }
  }
}

// ==========================================
// MQTT HEARTBEAT REPORTING
// ==========================================
unsigned long lastHeartbeat = 0;
void publishHeartbeat() {
  unsigned long now = millis();
  if (now - lastHeartbeat >= 5000 || lastHeartbeat == 0) {
    lastHeartbeat = now;
    if (mqttClient.connected()) {
      mqttClient.publish("rover/logger/heartbeat", "ONLINE", true);
      Serial.println("MQTT Publish Logger Heartbeat: ONLINE");
    }
  }
}

// ==========================================
// PERIODIC DATA LOGGING TO SD CARD
// ==========================================
// ==========================================
// DANGER STATE EVALUATION & TELEGRAM ALERTS
// ==========================================
void checkDangerStatus() {
  // Danger triggers
  bool fireDanger = (fire_status == "ON");
  bool tempDanger = (temp > 45.0);
  bool gasDanger = (mq2_val > 550) || (mq9_val > 2200) || (mq135_val > 3300);
  
  // Status check for fallback danger classifications
  bool statusDanger = (gas_overall_status.indexOf("DANGER") != -1) || 
                       (gas_overall_status.indexOf("FIRE") != -1) || 
                       (gas_overall_status.indexOf("FLAMMABLE") != -1) ||
                       (gas_overall_status.indexOf("POLLUTED") != -1) ||
                       (gas_overall_status.indexOf("HEAVY") != -1);
                       
  isDanger = fireDanger || tempDanger || gasDanger || statusDanger;
  
  unsigned long now = millis();
  if (isDanger) {
    // Send Telegram alert on initial transition, or repeat every 5 minutes (300000ms) if danger persists
    if (!lastDangerState || (now - lastTelegramAlertTime > 300000)) {
      lastTelegramAlertTime = now;
      
      String alertMsg = "⚠️ *CRITICAL DISASTER ROVER ALERT* ⚠️\n\n";
      alertMsg += "🚨 *Status:* DANGER LEVEL DETECTED!\n";
      if (fireDanger) alertMsg += "🔥 *Fire Alert:* ACTIVE!\n";
      if (tempDanger) alertMsg += "🌡️ *High Temp:* " + String(temp, 1) + " °C\n";
      if (mq2_val > 550) alertMsg += "💨 *MQ2 Smoke/LPG:* " + String(mq2_val, 0) + " (DANGEROUS)\n";
      if (mq9_val > 2200) alertMsg += "💨 *MQ9 Flammable Gas:* " + String(mq9_val, 0) + " (DANGEROUS)\n";
      if (mq135_val > 3300) alertMsg += "💨 *MQ135 Air Quality:* " + String(mq135_val, 0) + " (POLLUTED)\n";
      alertMsg += "\n📈 *Telemetry Summary:*\n";
      alertMsg += "• Temp/Hum: " + String(temp, 1) + " °C / " + String(hum, 1) + " %\n";
      alertMsg += "• Gas Status: " + gas_overall_status + "\n";
      alertMsg += "• Engine Status: " + last_motor_cmd + "\n";
      if (gps_valid) {
        alertMsg += "📍 [View Location](https://maps.google.com/?q=" + String(gps_lat, 6) + "," + String(gps_lon, 6) + ")\n";
      } else {
        alertMsg += "📍 *GPS:* No GNSS Fix\n";
      }
      bot.sendMessage(String(chatID), alertMsg, "Markdown");
      Serial.println("Telegram Alert Sent: DANGER!");

      // Log alert to Firebase Realtime Database alerts.json
      if (!lastDangerState && firebaseHost != NULL && strlen(firebaseHost) > 10) {
        WiFiClientSecure httpsClient;
        httpsClient.setInsecure();
        HTTPClient http;
        String alertsUrl = String(firebaseHost) + "alerts.json";
        http.begin(httpsClient, alertsUrl);
        http.addHeader("Content-Type", "application/json");
        
        String alertType = "Danger Alert";
        if (fireDanger) alertType = "Fire Hazard";
        else if (tempDanger) alertType = "Extreme Heat Alert";
        else if (gasDanger) alertType = "Toxic Gas Leak";

        String alertPayload = "{\"timestamp\":" + String(now / 1000) + 
                              ",\"type\":\"" + alertType + "\"" + 
                              ",\"temp\":" + String(temp, 1) + 
                              ",\"fire\":\"" + fire_status + "\"" + 
                              ",\"gas_status\":\"" + gas_overall_status + "\"}";
        int httpResponseCode = http.POST(alertPayload);
        http.end();
        Serial.printf("Firebase Alert Logged: (%d)\n", httpResponseCode);
      }
    }
  } else {
    // Send Telegram notification when transitioning from Danger back to Safe
    if (lastDangerState) {
      String resolveMsg = "✅ *DISASTER ROVER STATUS RESOLVED* ✅\n\n";
      resolveMsg += "Environment has returned to a SAFE level.\n\n";
      resolveMsg += "📈 *Current Telemetry:*\n";
      resolveMsg += "• Temp/Hum: " + String(temp, 1) + " °C / " + String(hum, 1) + " %\n";
      resolveMsg += "• Gas Status: " + gas_overall_status + "\n";
      resolveMsg += "• Fire Alert: OFF\n";
      bot.sendMessage(String(chatID), resolveMsg, "Markdown");
      Serial.println("Telegram Alert Sent: SAFE (Resolved)");
    }
  }
  lastDangerState = isDanger;
}

unsigned long lastLogTime = 0;
void logDataToSD() {
  if (!sdMounted) return;

  unsigned long now = millis();
  unsigned long interval = isDanger ? 10000 : 60000; // 10s if danger, 60s if safe
  
  if (now - lastLogTime >= interval || lastLogTime == 0) {
    lastLogTime = now;
    
    File logFile = SD.open("/logs.csv", FILE_APPEND);
    if (logFile) {
      // If file is newly created, append headers
      if (logFile.size() == 0) {
        logFile.println("Uptime(s),Temperature(C),Humidity(%),Latitude,Longitude,Speed(km/h),Satellites,FireAlert,LastEngineStatus,MQ2,MQ9,MQ135,GasOverallStatus");
      }
      
      logFile.printf("%lu,%.1f,%.1f,%.6f,%.6f,%.1f,%d,%s,%s,%.1f,%.1f,%.1f,%s\n",
                     now / 1000,
                     temp,
                     hum,
                     gps_lat,
                     gps_lon,
                     gps_speed,
                     gps_sat,
                     fire_status.c_str(),
                     last_motor_cmd.c_str(),
                     mq2_val,
                     mq9_val,
                     mq135_val,
                     gas_overall_status.c_str());
                     
      logFile.close();
      Serial.println("Telemetry entry logged to SD card (/logs.csv).");
    } else {
      Serial.println("ERROR: Failed to open /logs.csv on SD card for appending!");
    }
  }
}

// ==========================================
// NON-BLOCKING MQTT CONNECTION
// ==========================================
unsigned long lastMqttAttempt = 0;
void handleMqtt() {
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttAttempt > 5000 || lastMqttAttempt == 0) {
      lastMqttAttempt = now;
      Serial.print("Connecting to MQTT server [");
      Serial.print(mqtt_server);
      Serial.print("]...");
      
      String clientId = "ESP32_Rover_2_Logger_" + String(random(0xffff), HEX);
      if (mqttClient.connect(clientId.c_str())) {
        Serial.println(" Connected!");
        mqttClient.subscribe("rover/sensors/dht");
        mqttClient.subscribe("rover/sensors/gps");
        mqttClient.subscribe("rover/sensors/fire");
        mqttClient.subscribe("rover/sensors/gas");
        mqttClient.subscribe("rover/control/status");
      } else {
        Serial.print(" Connection failed, state = ");
        Serial.println(mqttClient.state());
      }
    }
  } else {
    mqttClient.loop();
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Mount SD Card
  Serial.print("Initializing SD card...");
  // Standard VSPI pins for ESP32 (CS=5, SCK=18, MISO=19, MOSI=23)
  if (SD.begin(5)) {
    Serial.println(" mounted successfully.");
    sdMounted = true;
  } else {
    Serial.println(" mounting FAILED! Logging is disabled.");
    sdMounted = false;
  }

  // WiFi Connection
  WiFi.begin(ssid, password);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed! Starting AP fallback...");
    WiFi.softAP("Rover-Vault-AP", "12345678");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  }

  // Telegram Insecure Client Setup (Skip certificate verification for lightweight SSL)
  securedClient.setInsecure();

  // MQTT Client callback setup
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // Web routes
  server.on("/", HTTP_GET, []() {
    String html = FPSTR(webpage);
    html.replace("%FILE_LIST%", getFilesTable());
    server.send(200, "text/html", html);
  });

  // AJAX route for live dashboard telemetry
  server.on("/api/telemetry", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String json = "{";
    json += "\"temp\":" + String(temp, 1) + ",";
    json += "\"hum\":" + String(hum, 1) + ",";
    json += "\"lat\":" + String(gps_lat, 6) + ",";
    json += "\"lon\":" + String(gps_lon, 6) + ",";
    json += "\"speed\":" + String(gps_speed, 1) + ",";
    json += "\"sat\":" + String(gps_sat) + ",";
    json += "\"gpsValid\":" + String(gps_valid ? "true" : "false") + ",";
    json += "\"fire\":\"" + fire_status + "\",";
    json += "\"motor\":\"" + last_motor_cmd + "\",";
    json += "\"mq2\":" + String(mq2_val, 1) + ",";
    json += "\"mq9\":" + String(mq9_val, 1) + ",";
    json += "\"mq135\":" + String(mq135_val, 1) + ",";
    json += "\"gasStatus\":\"" + gas_overall_status + "\",";
    json += "\"isDanger\":" + String(isDanger ? "true" : "false") + ",";
    json += "\"sdMounted\":" + String(sdMounted ? "true" : "false") + ",";
    json += "\"mqtt\":" + String(mqttClient.connected() ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
  });

  // API JSON files list route for GCS Storage Explorer
  server.on("/api/files", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!sdMounted) {
      server.send(500, "application/json", "[]");
      return;
    }
    String json = "[";
    File root = SD.open("/");
    if (root) {
      File file = root.openNextFile();
      bool first = true;
      while (file) {
        if (!file.isDirectory()) {
          if (!first) json += ",";
          first = false;
          String name = String(file.name());
          if (name.startsWith("/")) name = name.substring(1);
          json += "{\"name\":\"" + name + "\",\"size\":" + String(file.size()) + "}";
        }
        file = root.openNextFile();
      }
      root.close();
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  // Download file endpoint
  server.on("/download", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!sdMounted) {
      server.send(500, "text/plain", "SD card not mounted");
      return;
    }
    String path = server.arg("file");
    if (!path.startsWith("/")) path = "/" + path;
    
    if (SD.exists(path)) {
      File file = SD.open(path, FILE_READ);
      server.streamFile(file, "application/octet-stream");
      file.close();
    } else {
      server.send(404, "text/plain", "File Not Found");
    }
  });

  // View file text content endpoint
  server.on("/view", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!sdMounted) {
      server.send(500, "text/plain", "SD card not mounted");
      return;
    }
    String path = server.arg("file");
    if (!path.startsWith("/")) path = "/" + path;
    
    if (SD.exists(path)) {
      File file = SD.open(path, FILE_READ);
      server.setContentLength(file.size());
      server.send(200, "text/plain", "");
      
      uint8_t buffer[512];
      while (file.available()) {
        int bytesRead = file.read(buffer, sizeof(buffer));
        server.client().write(buffer, bytesRead);
      }
      file.close();
    } else {
      server.send(404, "text/plain", "File Not Found");
    }
  });

  // Delete file endpoint
  server.on("/delete", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!sdMounted) {
      server.send(500, "text/plain", "SD card not mounted");
      return;
    }
    String path = server.arg("file");
    if (!path.startsWith("/")) path = "/" + path;
    
    if (SD.exists(path)) {
      SD.remove(path);
      if (server.hasArg("api")) {
        server.send(200, "text/plain", "DELETED");
      } else {
        server.send(200, "text/html", "<script>alert('File deleted successfully.'); window.location.href='/';</script>");
      }
    } else {
      server.send(404, "text/plain", "File Not Found");
    }
  });

  server.begin();
  Serial.println("Vault Server started.");
}

// ==========================================
// CLOUD BLACKBOX DATABASE LOGGING (Firebase)
// ==========================================
unsigned long lastCloudLog = 0;
void logToCloud() {
  // If host URL is empty or too short, do nothing
  if (firebaseHost == NULL || strlen(firebaseHost) < 10) return;
  
  unsigned long now = millis();
  unsigned long interval = isDanger ? 10000 : 60000; // 10s if danger, 60s if safe
  
  if (now - lastCloudLog >= interval || lastCloudLog == 0) {
    lastCloudLog = now;
    
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    HTTPClient http;
    
    // Format JSON payload
    String json = "{";
    json += "\"timestamp\":" + String(now / 1000) + ",";
    json += "\"temp\":" + String(temp, 1) + ",";
    json += "\"hum\":" + String(hum, 1) + ",";
    json += "\"lat\":" + String(gps_lat, 6) + ",";
    json += "\"lon\":" + String(gps_lon, 6) + ",";
    json += "\"speed\":" + String(gps_speed, 1) + ",";
    json += "\"sat\":" + String(gps_sat) + ",";
    json += "\"fire\":\"" + fire_status + "\",";
    json += "\"motor\":\"" + last_motor_cmd + "\",";
    json += "\"mq2\":" + String(mq2_val, 1) + ",";
    json += "\"mq9\":" + String(mq9_val, 1) + ",";
    json += "\"mq135\":" + String(mq135_val, 1) + ",";
    json += "\"gas_status\":\"" + gas_overall_status + "\",";
    json += "\"is_danger\":" + String(isDanger ? "true" : "false");
    json += "}";
    
    // 1. Update the "Last Known State" (Realtime State)
    String lastKnownUrl = String(firebaseHost) + "last_known.json";
    http.begin(httpsClient, lastKnownUrl);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.PATCH(json);
    if (httpResponseCode > 0) {
      Serial.printf("Firebase Realtime State: Update success (%d)\n", httpResponseCode);
    } else {
      Serial.printf("Firebase Realtime State: Update error (%s)\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
    
    // 2. Append to "Flight Logs History" (Optional trail logging)
    String historyUrl = String(firebaseHost) + "history.json";
    http.begin(httpsClient, historyUrl);
    http.addHeader("Content-Type", "application/json");
    int httpPostCode = http.POST(json);
    http.end();
  }
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  server.handleClient();
  handleMqtt();
  checkDangerStatus();
  logDataToSD();
  logToCloud();
  publishHeartbeat();
  
  // Telegram Bot Updates check
  if (millis() - lastTelegramPoll > 1000) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTelegramPoll = millis();
  }
  
  // Periodic live location
  checkTelegramLiveLocation();
}
