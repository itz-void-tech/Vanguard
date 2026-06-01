/*
   =====================================================
   ESP32 RESCUE ROVER SMART SIREN WEB SERVER
   FULLY COMPATIBLE WITH ESP32 CORE 2.0.17
   =====================================================

   FEATURES:
   - Gas Toxic Alarm
   - Human Detection Beep
   - Fire Siren
   - Battery Low Warning
   - SOS Morse Code
   - Autonomous Startup Tone
   - Obstacle Danger Alert
   - Web Control Interface

   =====================================================
   CONNECTIONS
   =====================================================

   PASSIVE BUZZER MODULE:

   VCC  -> 3.3V
   GND  -> GND
   I/O  -> GPIO 25

   OR

   2 PIN PASSIVE BUZZER:

   + -> GPIO 25
   - -> GND

   =====================================================
*/

#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// WIFI ACCESS POINT
// =====================================================

const char* ssid = "ESP32-SIREN";
const char* password = "12345678";

// =====================================================
// BUZZER CONFIG
// =====================================================

#define BUZZER_PIN 25
#define BUZZER_CHANNEL 0

// =====================================================

WebServer server(80);

// =====================================================
// STOP SOUND
// =====================================================

void stopSound() {
  ledcWriteTone(BUZZER_CHANNEL, 0);
}

// =====================================================
// GAS TOXIC ALARM
// =====================================================

void gasAlarm() {

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

// =====================================================
// HUMAN DETECTED BEEP
// =====================================================

void humanDetected() {

  for (int i = 0; i < 2; i++) {

    ledcWriteTone(BUZZER_CHANNEL, 1200);
    delay(150);

    stopSound();
    delay(100);

    ledcWriteTone(BUZZER_CHANNEL, 1600);
    delay(150);

    stopSound();
    delay(200);
  }

  stopSound();
}

// =====================================================
// FIRE SIREN
// =====================================================

void fireSiren() {

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

// =====================================================
// BATTERY LOW WARNING
// =====================================================

void batteryLow() {

  for (int i = 0; i < 5; i++) {

    ledcWriteTone(BUZZER_CHANNEL, 800);
    delay(120);

    stopSound();
    delay(250);
  }

  stopSound();
}

// =====================================================
// SOS MORSE
// =====================================================

void dot() {

  ledcWriteTone(BUZZER_CHANNEL, 1500);
  delay(150);

  stopSound();
  delay(120);
}

void dash() {

  ledcWriteTone(BUZZER_CHANNEL, 1500);
  delay(450);

  stopSound();
  delay(120);
}

void sosMode() {

  // S
  dot();
  dot();
  dot();

  delay(250);

  // O
  dash();
  dash();
  dash();

  delay(250);

  // S
  dot();
  dot();
  dot();

  stopSound();
}

// =====================================================
// AUTONOMOUS STARTUP TONE
// =====================================================

void startupTone() {

  int melody[] = {700, 900, 1200, 1600, 2000};

  for (int i = 0; i < 5; i++) {

    ledcWriteTone(BUZZER_CHANNEL, melody[i]);
    delay(120);
  }

  stopSound();
}

// =====================================================
// OBSTACLE ALERT
// =====================================================

void obstacleAlert() {

  for (int i = 0; i < 25; i++) {

    ledcWriteTone(BUZZER_CHANNEL, 2400);
    delay(40);

    stopSound();
    delay(40);
  }

  stopSound();
}

// =====================================================
// HTML WEBPAGE
// =====================================================

String webpage = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<title>ESP32 Smart Siren</title>

<meta name="viewport" content="width=device-width, initial-scale=1">

<style>

body{
    background:#0f172a;
    font-family:Arial;
    text-align:center;
    color:white;
    margin:0;
    padding:20px;
}

h1{
    margin-bottom:30px;
    font-size:32px;
}

button{
    width:90%;
    max-width:400px;
    padding:20px;
    margin:12px;
    border:none;
    border-radius:18px;
    font-size:20px;
    font-weight:bold;
    cursor:pointer;
    transition:0.2s;
}

button:hover{
    transform:scale(1.03);
}

.gas{
    background:#ef4444;
    color:white;
}

.human{
    background:#22c55e;
    color:white;
}

.fire{
    background:#ff6b00;
    color:white;
}

.battery{
    background:#eab308;
    color:black;
}

.sos{
    background:#8b5cf6;
    color:white;
}

.auto{
    background:#06b6d4;
    color:white;
}

.obstacle{
    background:#f43f5e;
    color:white;
}

.stop{
    background:white;
    color:black;
}

</style>

</head>

<body>

<h1>🚨 ESP32 Rescue Rover Sirens</h1>

<button class="gas" onclick="send('/gas')">
☣ GAS TOXIC ALARM
</button>

<button class="human" onclick="send('/human')">
🧍 HUMAN DETECTED
</button>

<button class="fire" onclick="send('/fire')">
🔥 FIRE SIREN
</button>

<button class="battery" onclick="send('/battery')">
🔋 BATTERY LOW
</button>

<button class="sos" onclick="send('/sos')">
🆘 SOS MORSE
</button>

<button class="auto" onclick="send('/auto')">
🤖 AUTONOMOUS STARTUP
</button>

<button class="obstacle" onclick="send('/obstacle')">
⚠ OBSTACLE ALERT
</button>

<button class="stop" onclick="send('/stop')">
⛔ STOP SOUND
</button>

<script>

function send(url){

    fetch(url)
    .then(response => console.log("OK"))

}

</script>

</body>
</html>

)rawliteral";

// =====================================================
// WEB ROUTES
// =====================================================

void handleRoot() {

  server.send(200, "text/html", webpage);
}

void setupRoutes() {

  server.on("/", handleRoot);

  server.on("/gas", []() {

    server.send(200, "text/plain", "Gas Alarm");

    gasAlarm();
  });

  server.on("/human", []() {

    server.send(200, "text/plain", "Human Detected");

    humanDetected();
  });

  server.on("/fire", []() {

    server.send(200, "text/plain", "Fire Siren");

    fireSiren();
  });

  server.on("/battery", []() {

    server.send(200, "text/plain", "Battery Low");

    batteryLow();
  });

  server.on("/sos", []() {

    server.send(200, "text/plain", "SOS");

    sosMode();
  });

  server.on("/auto", []() {

    server.send(200, "text/plain", "Autonomous Mode");

    startupTone();
  });

  server.on("/obstacle", []() {

    server.send(200, "text/plain", "Obstacle Alert");

    obstacleAlert();
  });

  server.on("/stop", []() {

    server.send(200, "text/plain", "Stopped");

    stopSound();
  });
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // PWM SETUP
  ledcSetup(BUZZER_CHANNEL, 2000, 8);

  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);

  stopSound();

  // START ACCESS POINT
  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("==================================");
  Serial.println("ESP32 SMART SIREN SERVER STARTED");
  Serial.println("==================================");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("==================================");

  setupRoutes();

  server.begin();

  // STARTUP SOUND
  startupTone();
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  server.handleClient();
}