/*
   ================================================================================
   ESP32 ROVER - SYSTEM 1 (CONTROL & SIREN)
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
   DEVICE CONFIGURATION (ESP1 SPECIFICS):
   - BTS7960 Motor Driver:
     - Left Motor RPWM  -> GPIO 14 (Channel 0)
     - Left Motor LPWM  -> GPIO 12 (Channel 1)
     - Right Motor RPWM -> GPIO 27 (Channel 2)
     - Right Motor LPWM -> GPIO 26 (Channel 3)
   - Passive Buzzer:
     - Pin I/O          -> GPIO 25 (Channel 4)
   - Pan-Tilt Servos:
     - Pan Servo        -> GPIO 23
     - Tilt Servo       -> GPIO 22
   - Robotic Arm Servos:
     - Arm Base Servo   -> GPIO 21
     - Arm Shoulder     -> GPIO 19
     - Arm Elbow        -> GPIO 18
     - Arm Gripper      -> GPIO 5
   - Ultrasonic Sensors:
     - Front Ultrasonic -> TRIG: GPIO 4, ECHO: GPIO 2
     - Back Ultrasonic  -> TRIG: GPIO 15, ECHO: GPIO 13

   MQTT TOPICS:
   - Subscribed Topics:
     - "rover/control/cmd"    (Payload: F, B, L, R, S)
     - "rover/control/speed"  (Payload: 0 to 255)
     - "rover/siren/cmd"      (Payload: gas, human, fire, battery, sos, auto, obstacle, stop)
     - "rover/arm/cmd"        (Payload: JSON config for base, shoulder, elbow, gripper angles)
     - "rover/pantilt/cmd"    (Payload: JSON config for pan, tilt angles)
   - Published Topics:
     - "rover/control/status" (Payload: Current motion status / Speed)
     - "rover/siren/status"   (Payload: Currently active siren)

   ================================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

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
// MOTOR DRIVER PINS & PWM SETUP
// ==========================================
#define L_RPWM 12
#define L_LPWM 14
#define R_RPWM 26
#define R_LPWM 27

#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

#define L_RPWM_CH 0
#define L_LPWM_CH 1
#define R_RPWM_CH 2
#define R_LPWM_CH 3

int motorSpeed = 180;
String currentMovement = "STOPPED";

// ==========================================
// BUZZER PINS & CONFIG
// ==========================================
#define BUZZER_PIN 25
#define BUZZER_CHANNEL 4 // Channel 4 avoids overlap with motors (0-3)

String currentSiren = "NONE";

// ==========================================
// SERVO CONFIGURATION & OBJECTS
// ==========================================
#define ARM_BASE_PIN 21
#define ARM_SHOULDER_PIN 19
#define ARM_ELBOW_PIN 18
#define ARM_GRIPPER_PIN 5
#define PT_PAN_PIN 23
#define PT_TILT_PIN 22

Servo armBaseServo;
Servo armShoulderServo;
Servo armElbowServo;
Servo armGripperServo;
Servo ptPanServo;
Servo ptTiltServo;

// ==========================================
// MOTOR CONTROL FUNCTIONS
// ==========================================
void stopMotors() {
  ledcWrite(L_RPWM_CH, 0);
  ledcWrite(L_LPWM_CH, 0);
  ledcWrite(R_RPWM_CH, 0);
  ledcWrite(R_LPWM_CH, 0);
  currentMovement = "STOPPED";
  publishStatus();
}

void moveForward() {
  ledcWrite(L_RPWM_CH, motorSpeed);
  ledcWrite(L_LPWM_CH, 0);
  ledcWrite(R_RPWM_CH, motorSpeed);
  ledcWrite(R_LPWM_CH, 0);
  currentMovement = "FORWARD";
  publishStatus();
}

void moveBackward() {
  ledcWrite(L_RPWM_CH, 0);
  ledcWrite(L_LPWM_CH, motorSpeed);
  ledcWrite(R_RPWM_CH, 0);
  ledcWrite(R_LPWM_CH, motorSpeed);
  currentMovement = "BACKWARD";
  publishStatus();
}

void turnLeft() {
  ledcWrite(L_RPWM_CH, 0);
  ledcWrite(L_LPWM_CH, motorSpeed);
  ledcWrite(R_RPWM_CH, motorSpeed);
  ledcWrite(R_LPWM_CH, 0);
  currentMovement = "LEFT";
  publishStatus();
}

void turnRight() {
  ledcWrite(L_RPWM_CH, motorSpeed);
  ledcWrite(L_LPWM_CH, 0);
  ledcWrite(R_RPWM_CH, 0);
  ledcWrite(R_LPWM_CH, motorSpeed);
  currentMovement = "RIGHT";
  publishStatus();
}

// ==========================================
// SIREN FUNCTIONS (Identical to original)
// ==========================================
void stopSound() {
  ledcWriteTone(BUZZER_CHANNEL, 0);
  currentSiren = "NONE";
  publishSirenStatus();
}

void gasAlarm() {
  currentSiren = "GAS TOXIC ALARM";
  publishSirenStatus();
  for (int i = 0; i < 3; i++) {
    for (int freq = 700; freq <= 2200; freq += 15) {
      ledcWriteTone(BUZZER_CHANNEL, freq);
      delay(2);
    }
    for (int freq = 2200; freq >= 700; freq -= 15) {
      ledcWriteTone(BUZZER_CHANNEL, freq);
      delay(2);
    }
  }
  stopSound();
}

void humanDetected() {
  currentSiren = "HUMAN DETECTED BEEP";
  publishSirenStatus();
  for (int i = 0; i < 2; i++) {
    ledcWriteTone(BUZZER_CHANNEL, 1200);
    delay(150);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    delay(100);
    ledcWriteTone(BUZZER_CHANNEL, 1600);
    delay(150);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    delay(200);
  }
  stopSound();
}

void fireSiren() {
  currentSiren = "FIRE SIREN";
  publishSirenStatus();
  for (int i = 0; i < 4; i++) {
    for (int freq = 500; freq <= 2000; freq += 8) {
      ledcWriteTone(BUZZER_CHANNEL, freq);
      delay(3);
    }
    for (int freq = 2000; freq >= 500; freq -= 8) {
      ledcWriteTone(BUZZER_CHANNEL, freq);
      delay(3);
    }
  }
  stopSound();
}

void batteryLow() {
  currentSiren = "BATTERY LOW WARNING";
  publishSirenStatus();
  for (int i = 0; i < 5; i++) {
    ledcWriteTone(BUZZER_CHANNEL, 800);
    delay(120);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    delay(250);
  }
  stopSound();
}

void dot() {
  ledcWriteTone(BUZZER_CHANNEL, 1500);
  delay(150);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  delay(120);
}

void dash() {
  ledcWriteTone(BUZZER_CHANNEL, 1500);
  delay(450);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  delay(120);
}

void sosMode() {
  currentSiren = "SOS MORSE ALARM";
  publishSirenStatus();
  // S (. . .)
  dot(); dot(); dot();
  delay(250);
  // O (- - -)
  dash(); dash(); dash();
  delay(250);
  // S (. . .)
  dot(); dot(); dot();
  stopSound();
}

void startupTone() {
  currentSiren = "STARTUP TONE";
  publishSirenStatus();
  int melody[] = {700, 900, 1200, 1600, 2000};
  for (int i = 0; i < 5; i++) {
    ledcWriteTone(BUZZER_CHANNEL, melody[i]);
    delay(120);
  }
  stopSound();
}

void obstacleAlert() {
  currentSiren = "OBSTACLE ALERT";
  publishSirenStatus();
  for (int i = 0; i < 25; i++) {
    ledcWriteTone(BUZZER_CHANNEL, 2400);
    delay(40);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    delay(40);
  }
  stopSound();
}

// ==========================================
// MQTT REPORTING
// ==========================================
unsigned long lastHeartbeat = 0;
void publishHeartbeat() {
  unsigned long now = millis();
  if (now - lastHeartbeat >= 5000 || lastHeartbeat == 0) {
    lastHeartbeat = now;
    if (mqttClient.connected()) {
      mqttClient.publish("rover/control/heartbeat", "ONLINE", true);
      Serial.println("MQTT Publish Heartbeat: ONLINE");
    }
  }
}

void publishStatus() {
  String msg = "MOTOR:" + currentMovement + " | SPEED:" + String(motorSpeed);
  mqttClient.publish("rover/control/status", msg.c_str(), true);
  Serial.println("MQTT Publish Control: " + msg);
}

void publishSirenStatus() {
  mqttClient.publish("rover/siren/status", currentSiren.c_str(), true);
  Serial.println("MQTT Publish Siren: " + currentSiren);
}

// ==========================================
// WEB SERVER HTML DASHBOARD (Cyberpunk Slate Theme)
// ==========================================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Rover Mission Control</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-color: #0b0f19;
            --card-bg: rgba(17, 24, 39, 0.7);
            --accent-cyan: #06b6d4;
            --accent-purple: #8b5cf6;
            --accent-red: #ef4444;
            --accent-orange: #f97316;
            --text-color: #f3f4f6;
            --border-glow: rgba(6, 182, 212, 0.15);
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            background-color: var(--bg-color);
            background-image: 
                radial-gradient(at 10% 20%, rgba(139, 92, 246, 0.15) 0px, transparent 50%),
                radial-gradient(at 90% 80%, rgba(6, 182, 212, 0.15) 0px, transparent 50%);
            color: var(--text-color);
            font-family: 'Outfit', sans-serif;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 30px 15px;
        }
        header {
            text-align: center;
            margin-bottom: 30px;
        }
        header h1 {
            font-size: 2.5rem;
            font-weight: 800;
            background: linear-gradient(135deg, var(--accent-cyan), var(--accent-purple));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            letter-spacing: 2px;
            text-transform: uppercase;
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
        @media(min-width: 800px) {
            .container {
                grid-template-columns: 1fr 1fr;
            }
        }
        .card {
            background: var(--card-bg);
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 24px;
            padding: 30px;
            backdrop-filter: blur(16px);
            box-shadow: 0 10px 30px -10px rgba(0, 0, 0, 0.5), inset 0 1px 0 rgba(255, 255, 255, 0.1);
            transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
        }
        .card:hover {
            border-color: rgba(6, 182, 212, 0.3);
            box-shadow: 0 15px 35px -10px rgba(6, 182, 212, 0.1), 0 0 1px rgba(6, 182, 212, 0.2);
        }
        .card-title {
            font-size: 1.3rem;
            font-weight: 600;
            margin-bottom: 25px;
            display: flex;
            align-items: center;
            gap: 10px;
            color: var(--accent-cyan);
            border-bottom: 1px solid rgba(255, 255, 255, 0.08);
            padding-bottom: 12px;
        }
        /* D-Pad Motor Control UI */
        .control-grid {
            display: grid;
            grid-template-columns: repeat(3, 90px);
            grid-template-rows: repeat(3, 90px);
            gap: 15px;
            justify-content: center;
            margin: 20px 0;
        }
        .btn-dir {
            background: rgba(30, 41, 59, 0.7);
            border: 1px solid rgba(6, 182, 212, 0.2);
            border-radius: 20px;
            color: var(--text-color);
            font-size: 1.8rem;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            box-shadow: 0 4px 12px rgba(0,0,0,0.2);
        }
        .btn-dir:hover {
            background: var(--accent-cyan);
            color: #0b0f19;
            transform: translateY(-2px);
            box-shadow: 0 0 20px rgba(6, 182, 212, 0.4);
        }
        .btn-dir:active {
            transform: translateY(1px);
        }
        .btn-stop {
            grid-column: 2;
            grid-row: 2;
            background: rgba(239, 68, 68, 0.15);
            border: 1px solid var(--accent-red);
            color: var(--accent-red);
            font-size: 0.95rem;
            font-weight: 800;
            letter-spacing: 1px;
        }
        .btn-stop:hover {
            background: var(--accent-red);
            color: var(--text-color);
            box-shadow: 0 0 20px rgba(239, 68, 68, 0.4);
        }
        /* Slider styling */
        .slider-container {
            margin-top: 30px;
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        .slider-label {
            display: flex;
            justify-content: space-between;
            font-size: 0.9rem;
            color: #9ca3af;
        }
        .slider-val {
            font-weight: 600;
            color: var(--accent-cyan);
        }
        .range-slider {
            width: 100%;
            height: 8px;
            border-radius: 4px;
            background: #1e293b;
            outline: none;
            -webkit-appearance: none;
        }
        .range-slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 22px;
            height: 22px;
            border-radius: 50%;
            background: var(--accent-cyan);
            cursor: pointer;
            box-shadow: 0 0 10px rgba(6, 182, 212, 0.5);
            transition: transform 0.1s;
        }
        .range-slider::-webkit-slider-thumb:hover {
            transform: scale(1.2);
        }
        /* Sirens Grid Control */
        .sirens-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
        }
        .btn-siren {
            padding: 15px;
            border-radius: 16px;
            border: 1px solid rgba(255, 255, 255, 0.05);
            font-size: 0.85rem;
            font-weight: 600;
            letter-spacing: 0.5px;
            color: white;
            cursor: pointer;
            transition: all 0.2s ease;
            text-align: center;
            box-shadow: 0 4px 10px rgba(0,0,0,0.15);
        }
        .btn-siren:hover {
            transform: translateY(-2px);
            filter: brightness(1.1);
        }
        .btn-siren.gas { background: linear-gradient(135deg, #dc2626, #ef4444); border-color: #f87171; }
        .btn-siren.human { background: linear-gradient(135deg, #16a34a, #22c55e); border-color: #4ade80; }
        .btn-siren.fire { background: linear-gradient(135deg, #ea580c, #f97316); border-color: #fb923c; }
        .btn-siren.battery { background: linear-gradient(135deg, #ca8a04, #eab308); border-color: #facc15; color: #0b0f19; }
        .btn-siren.sos { background: linear-gradient(135deg, #7c3aed, #8b5cf6); border-color: #a78bfa; }
        .btn-siren.auto { background: linear-gradient(135deg, #0284c7, #06b6d4); border-color: #22d3ee; }
        .btn-siren.obstacle { background: linear-gradient(135deg, #be123c, #fb7185); border-color: #fda4af; }
        .btn-siren.stop-sound {
            grid-column: span 2;
            background: #f3f4f6;
            color: #0b0f19;
            font-weight: bold;
            font-size: 0.95rem;
            margin-top: 8px;
        }
        .btn-siren.stop-sound:hover {
            box-shadow: 0 0 15px rgba(255, 255, 255, 0.4);
        }
        /* Status Card */
        .status-container {
            display: flex;
            flex-direction: column;
            gap: 15px;
        }
        .status-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            background: rgba(30, 41, 59, 0.4);
            padding: 15px 20px;
            border-radius: 14px;
            border: 1px solid rgba(255, 255, 255, 0.03);
        }
        .status-name {
            font-size: 0.9rem;
            color: #9ca3af;
        }
        .status-val {
            font-weight: 600;
            font-size: 0.95rem;
        }
        .status-val.highlight {
            color: var(--accent-cyan);
            text-shadow: 0 0 10px rgba(6, 182, 212, 0.3);
        }
        .status-val.danger {
            color: var(--accent-red);
            text-shadow: 0 0 10px rgba(239, 68, 68, 0.3);
        }
        .status-val.ok {
            color: #22c55e;
        }
    </style>
</head>
<body>
    <header>
        <h1>🌌 Rover Mission Control</h1>
        <p>ESP32 Integrated Drive & Alert System</p>
    </header>

    <div class="container">
        <!-- Motor Controls -->
        <div class="card">
            <div class="card-title">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
                Direction & Speed
            </div>
            
            <div class="control-grid">
                <button class="btn-dir" style="grid-column: 2; grid-row: 1;" onclick="sendCmd('F')">▲</button>
                <button class="btn-dir" style="grid-column: 1; grid-row: 2;" onclick="sendCmd('L')">◀</button>
                <button class="btn-stop btn-dir" onclick="sendCmd('S')">STOP</button>
                <button class="btn-dir" style="grid-column: 3; grid-row: 2;" onclick="sendCmd('R')">▶</button>
                <button class="btn-dir" style="grid-column: 2; grid-row: 3;" onclick="sendCmd('B')">▼</button>
            </div>

            <div class="slider-container">
                <div class="slider-label">
                    <span>Motor Output Power</span>
                    <span class="slider-val"><span id="speedVal">180</span>/255</span>
                </div>
                <input type="range" min="0" max="255" value="180" class="range-slider" oninput="updateSpeedVal(this.value)" onchange="setSpeed(this.value)">
            </div>
        </div>

        <!-- Siren Controls -->
        <div class="card">
            <div class="card-title">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9M13.73 21a2 2 0 0 1-3.46 0"/></svg>
                Siren Alerts & Alarms
            </div>
            <div class="sirens-grid">
                <button class="btn-siren gas" onclick="sendSiren('gas')">☣ TOXIC GAS</button>
                <button class="btn-siren human" onclick="sendSiren('human')">🧍 HUMAN DETECTED</button>
                <button class="btn-siren fire" onclick="sendSiren('fire')">🔥 FIRE DANGER</button>
                <button class="btn-siren battery" onclick="sendSiren('battery')">🔋 LOW BATTERY</button>
                <button class="btn-siren sos" onclick="sendSiren('sos')">🆘 SOS MORSE</button>
                <button class="btn-siren auto" onclick="sendSiren('auto')">🤖 STARTUP TONE</button>
                <button class="btn-siren obstacle" onclick="sendSiren('obstacle')">⚠ OBSTACLE ALERT</button>
                <button class="btn-siren stop-sound" onclick="sendSiren('stop')">⛔ MUTE SOUND</button>
            </div>
        </div>

        <!-- Status Dashboard -->
        <div class="card" style="grid-column: span 1; width: 100%;">
            <div class="card-title">
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"/><path d="M9 3v18M15 3v18M3 9h18M3 15h18"/></svg>
                System Telemetry
            </div>
            <div class="status-container">
                <div class="status-item">
                    <span class="status-name">Motion Status</span>
                    <span class="status-val highlight" id="statusMotion">STOPPED</span>
                </div>
                <div class="status-item">
                    <span class="status-name">Active Audio Alarm</span>
                    <span class="status-val danger" id="statusSiren">NONE</span>
                </div>
                <div class="status-item">
                    <span class="status-name">MQTT Broker Link</span>
                    <span class="status-val ok" id="statusMqtt">CONNECTING...</span>
                </div>
            </div>
        </div>
    </div>

    <script>
        function sendCmd(cmd) {
            fetch('/cmd?dir=' + cmd)
                .then(r => r.text())
                .then(txt => {
                    document.getElementById('statusMotion').innerText = getMotionWord(cmd);
                });
        }
        function getMotionWord(cmd) {
            if(cmd === 'F') return 'FORWARD';
            if(cmd === 'B') return 'BACKWARD';
            if(cmd === 'L') return 'LEFT TURN';
            if(cmd === 'R') return 'RIGHT TURN';
            return 'STOPPED';
        }
        function setSpeed(val) {
            fetch('/speed?val=' + val);
        }
        function updateSpeedVal(val) {
            document.getElementById('speedVal').innerText = val;
        }
        function sendSiren(type) {
            fetch('/siren/' + type)
                .then(r => r.text())
                .then(txt => {
                    document.getElementById('statusSiren').innerText = type.toUpperCase() === 'STOP' ? 'NONE' : type.toUpperCase();
                });
        }
        function checkTelemetry() {
            fetch('/status')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('statusMotion').innerText = data.motion;
                    document.getElementById('statusSiren').innerText = data.siren;
                    document.getElementById('statusMqtt').innerText = data.mqtt ? 'CONNECTED' : 'DISCONNECTED';
                    document.getElementById('statusMqtt').className = data.mqtt ? 'status-val ok' : 'status-val danger';
                });
        }
        setInterval(checkTelemetry, 2500);
    </script>
</body>
</html>
)rawliteral";

// ==========================================
// MQTT CALLBACK HANDLER
// ==========================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("MQTT Command [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == "rover/control/cmd") {
    if (message == "F") moveForward();
    else if (message == "B") moveBackward();
    else if (message == "L") turnLeft();
    else if (message == "R") turnRight();
    else if (message == "S") stopMotors();
  }
  else if (String(topic) == "rover/control/speed") {
    motorSpeed = message.toInt();
    if (motorSpeed < 0) motorSpeed = 0;
    if (motorSpeed > 255) motorSpeed = 255;
    publishStatus();
  }
  else if (String(topic) == "rover/siren/cmd") {
    if (message == "gas") gasAlarm();
    else if (message == "human") humanDetected();
    else if (message == "fire") fireSiren();
    else if (message == "battery") batteryLow();
    else if (message == "sos") sosMode();
    else if (message == "auto") startupTone();
    else if (message == "obstacle") obstacleAlert();
    else if (message == "stop") stopSound();
  }
  else if (String(topic) == "rover/arm/cmd") {
    int baseIdx = message.indexOf("\"base\":");
    if (baseIdx != -1) {
      int commaIdx = message.indexOf(",", baseIdx);
      if (commaIdx == -1) commaIdx = message.indexOf("}", baseIdx);
      int baseVal = message.substring(baseIdx + 7, commaIdx).toInt();
      armBaseServo.write(baseVal);
    }
    int shoulderIdx = message.indexOf("\"shoulder\":");
    if (shoulderIdx != -1) {
      int commaIdx = message.indexOf(",", shoulderIdx);
      if (commaIdx == -1) commaIdx = message.indexOf("}", shoulderIdx);
      int shoulderVal = message.substring(shoulderIdx + 11, commaIdx).toInt();
      armShoulderServo.write(shoulderVal);
    }
    int elbowIdx = message.indexOf("\"elbow\":");
    if (elbowIdx != -1) {
      int commaIdx = message.indexOf(",", elbowIdx);
      if (commaIdx == -1) commaIdx = message.indexOf("}", elbowIdx);
      int elbowVal = message.substring(elbowIdx + 8, commaIdx).toInt();
      armElbowServo.write(elbowVal);
    }
    int gripperIdx = message.indexOf("\"gripper\":");
    if (gripperIdx != -1) {
      int commaIdx = message.indexOf(",", gripperIdx);
      if (commaIdx == -1) commaIdx = message.indexOf("}", gripperIdx);
      int gripperVal = message.substring(gripperIdx + 10, commaIdx).toInt();
      armGripperServo.write(gripperVal);
    }
  }
  else if (String(topic) == "rover/pantilt/cmd") {
    int panIdx = message.indexOf("\"pan\":");
    if (panIdx != -1) {
      int commaIdx = message.indexOf(",", panIdx);
      if (commaIdx == -1) commaIdx = message.indexOf("}", panIdx);
      int panVal = message.substring(panIdx + 6, commaIdx).toInt();
      ptPanServo.write(panVal);
    }
    int tiltIdx = message.indexOf("\"tilt\":");
    if (tiltIdx != -1) {
      int commaIdx = message.indexOf(",", tiltIdx);
      if (commaIdx == -1) commaIdx = message.indexOf("}", tiltIdx);
      int tiltVal = message.substring(tiltIdx + 7, commaIdx).toInt();
      ptTiltServo.write(tiltVal);
    }
  }
}

// ==========================================
// NON-BLOCKING MQTT RECONNECT
// ==========================================
unsigned long lastMqttAttempt = 0;
void handleMqtt() {
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttAttempt > 5000 || lastMqttAttempt == 0) {
      lastMqttAttempt = now;
      Serial.print("Attempting MQTT connection to ");
      Serial.print(mqtt_server);
      Serial.print("...");
      
      String clientId = "ESP32_Rover_1_" + String(random(0xffff), HEX);
      if (mqttClient.connect(clientId.c_str())) {
        Serial.println(" Connected!");
        mqttClient.subscribe("rover/control/cmd");
        mqttClient.subscribe("rover/control/speed");
        mqttClient.subscribe("rover/siren/cmd");
        mqttClient.subscribe("rover/arm/cmd");
        mqttClient.subscribe("rover/pantilt/cmd");
        publishStatus();
        publishSirenStatus();
      } else {
        Serial.print(" Failed, rc=");
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

  // Motor Pins Setup
  ledcSetup(L_RPWM_CH, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(L_LPWM_CH, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(R_RPWM_CH, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(R_LPWM_CH, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(L_RPWM, L_RPWM_CH);
  ledcAttachPin(L_LPWM, L_LPWM_CH);
  ledcAttachPin(R_RPWM, R_RPWM_CH);
  ledcAttachPin(R_LPWM, R_LPWM_CH);

  // Buzzer Setup
  ledcSetup(BUZZER_CHANNEL, 2000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  
  stopMotors();
  stopSound();

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
    Serial.println("\nWiFi connection failed! Starting AP mode as fallback...");
    WiFi.softAP("Rover-Control-AP", "12345678");
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  }

  // MQTT Server Setup
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // Web routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", webpage);
  });

  server.on("/cmd", HTTP_GET, []() {
    String dir = server.arg("dir");
    if(dir == "F") moveForward();
    else if(dir == "B") moveBackward();
    else if(dir == "L") turnLeft();
    else if(dir == "R") turnRight();
    else if(dir == "S") stopMotors();
    server.send(200, "text/plain", "OK");
  });

  server.on("/speed", HTTP_GET, []() {
    motorSpeed = server.arg("val").toInt();
    if(motorSpeed < 0) motorSpeed = 0;
    if(motorSpeed > 255) motorSpeed = 255;
    publishStatus();
    server.send(200, "text/plain", "Speed updated");
  });

  // Siren direct triggering routes
  server.on("/siren/gas", HTTP_GET, []() {
    server.send(200, "text/plain", "Triggered gas alarm");
    gasAlarm();
  });
  server.on("/siren/human", HTTP_GET, []() {
    server.send(200, "text/plain", "Triggered human beep");
    humanDetected();
  });
  server.on("/siren/fire", HTTP_GET, []() {
    server.send(200, "text/plain", "Triggered fire siren");
    fireSiren();
  });
  server.on("/siren/battery", HTTP_GET, []() {
    server.send(200, "text/plain", "Triggered battery warning");
    batteryLow();
  });
  server.on("/siren/sos", HTTP_GET, []() {
    server.send(200, "text/plain", "Triggered SOS Morse");
    sosMode();
  });
  server.on("/siren/auto", HTTP_GET, []() {
    server.send(200, "text/plain", "Triggered Startup Tune");
    startupTone();
  });
  server.on("/siren/obstacle", HTTP_GET, []() {
    server.send(200, "text/plain", "Triggered obstacle alert");
    obstacleAlert();
  });
  server.on("/siren/stop", HTTP_GET, []() {
    server.send(200, "text/plain", "Sound stopped");
    stopSound();
  });

  // AJAX Telemetry route
  server.on("/status", HTTP_GET, []() {
    String json = "{";
    json += "\"motion\":\"" + currentMovement + "\",";
    json += "\"siren\":\"" + currentSiren + "\",";
    json += "\"mqtt\":" + String(mqttClient.connected() ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
  });

  // Configure Ultrasonic Pins
  pinMode(4, OUTPUT); // FRONT_TRIG
  pinMode(2, INPUT);  // FRONT_ECHO
  pinMode(15, OUTPUT); // BACK_TRIG
  pinMode(13, INPUT);  // BACK_ECHO

  // Configure Robotic Arm and Pan-Tilt Servos
  armBaseServo.setPeriodHertz(50);
  armShoulderServo.setPeriodHertz(50);
  armElbowServo.setPeriodHertz(50);
  armGripperServo.setPeriodHertz(50);
  ptPanServo.setPeriodHertz(50);
  ptTiltServo.setPeriodHertz(50);

  armBaseServo.attach(ARM_BASE_PIN, 500, 2400);
  armShoulderServo.attach(ARM_SHOULDER_PIN, 500, 2400);
  armElbowServo.attach(ARM_ELBOW_PIN, 500, 2400);
  armGripperServo.attach(ARM_GRIPPER_PIN, 500, 2400);
  ptPanServo.attach(PT_PAN_PIN, 500, 2400);
  ptTiltServo.attach(PT_TILT_PIN, 500, 2400);

  // Default neutral center positioning
  armBaseServo.write(90);
  armShoulderServo.write(90);
  armElbowServo.write(90);
  armGripperServo.write(90);
  ptPanServo.write(90);
  ptTiltServo.write(90);

  server.begin();
  Serial.println("HTTP Web Server running.");

  // Play a quick melody on successful startup
  startupTone();
}

// ==========================================
// ULTRASONIC OBSTACLE AVOIDANCE
// ==========================================
#define FRONT_TRIG 4
#define FRONT_ECHO 2
#define BACK_TRIG 15
#define BACK_ECHO 13

#define DISTANCE_THRESHOLD 20 // threshold in cm

long readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 20000); // 20ms timeout (~3.4m max)
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void shortObstacleBeep() {
  ledcWriteTone(BUZZER_CHANNEL, 2400);
  delay(100);
  ledcWriteTone(BUZZER_CHANNEL, 0);
}

unsigned long lastObstacleCheck = 0;
void checkObstacles() {
  unsigned long now = millis();
  if (now - lastObstacleCheck >= 100) {
    lastObstacleCheck = now;
    
    long frontDist = readDistance(FRONT_TRIG, FRONT_ECHO);
    long backDist = readDistance(BACK_TRIG, BACK_ECHO);
    
    if (currentMovement == "FORWARD" && frontDist < DISTANCE_THRESHOLD) {
      Serial.print("Obstacle FRONT! Dist: ");
      Serial.print(frontDist);
      Serial.println(" cm. Reversing...");
      shortObstacleBeep();
      moveBackward(); // Auto-reverse
    } 
    else if (currentMovement == "BACKWARD" && backDist < DISTANCE_THRESHOLD) {
      Serial.print("Obstacle BACK! Dist: ");
      Serial.print(backDist);
      Serial.println(" cm. Advancing...");
      shortObstacleBeep();
      moveForward(); // Auto-advance
    }
  }
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  server.handleClient();
  handleMqtt();
  checkObstacles();
  publishHeartbeat();
}
