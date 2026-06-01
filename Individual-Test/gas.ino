/*
========================================================
ESP32 SMART GAS MONITOR SYSTEM
========================================================

SENSORS:
- MQ2   -> Smoke / LPG / Fire Gas
- MQ9   -> CO / Flammable Gas
- MQ135 -> Air Quality / VOC

FEATURES:
✅ Smart calibrated thresholds
✅ Sensor averaging
✅ Noise reduction
✅ Live web dashboard
✅ Auto refresh
✅ Proper status detection
✅ Based on YOUR REAL SENSOR DATA

WIFI:
SSID     : sim
PASSWORD : simple12

========================================================
CONNECTIONS
========================================================

MQ2:
VCC -> 5V
GND -> GND
AO  -> GPIO34

MQ9:
VCC -> 5V
GND -> GND
AO  -> GPIO35

MQ135:
VCC -> 5V
GND -> GND
AO  -> GPIO32

IMPORTANT:
- Power sensors using 5V
- ESP32 GND must connect to sensor GND
- Let sensors warm up 5-10 mins

========================================================
*/

#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// WIFI
// =====================================================

const char* ssid = "sim";
const char* password = "simple12";

// =====================================================
// WEB SERVER
// =====================================================

WebServer server(80);

// =====================================================
// SENSOR PINS
// =====================================================

#define MQ2_PIN    34
#define MQ9_PIN    35
#define MQ135_PIN  32

// =====================================================
// SENSOR VALUES
// =====================================================

float mq2Value = 0;
float mq9Value = 0;
float mq135Value = 0;

// =====================================================
// STATUS VARIABLES
// =====================================================

String mq2Status = "";
String mq9Status = "";
String mq135Status = "";

String overallStatus = "";

// =====================================================
// READ AVERAGE FUNCTION
// =====================================================

float readAverage(int pin) {

  long total = 0;

  for (int i = 0; i < 20; i++) {

    total += analogRead(pin);

    delay(5);
  }

  return total / 20.0;
}

// =====================================================
// SENSOR ANALYSIS
// =====================================================

void analyzeSensors() {

  // =========================
  // MQ2 ANALYSIS
  // =========================

  if (mq2Value < 1200) {
    mq2Status = "SAFE";
  }
  else if (mq2Value >= 1200 && mq2Value < 2000) {
    mq2Status = "WARNING";
  }
  else {
    mq2Status = "DANGEROUS SMOKE/GAS";
  }

  // =========================
  // MQ9 ANALYSIS
  // =========================

  if (mq9Value < 1500) {
    mq9Status = "SAFE";
  }
  else if (mq9Value >= 1500 && mq9Value < 2200) {
    mq9Status = "GAS DETECTED";
  }
  else {
    mq9Status = "HIGH FLAMMABLE GAS";
  }

  // =========================
  // MQ135 ANALYSIS
  // =========================

  if (mq135Value < 1500) {
    mq135Status = "AIR QUALITY GOOD";
  }
  else if (mq135Value >= 1500 && mq135Value < 2500) {
    mq135Status = "POLLUTED AIR";
  }
  else {
    mq135Status = "HEAVY SMOKE / VOC";
  }

  // =================================================
  // OVERALL SYSTEM ANALYSIS
  // =================================================

  if (mq2Value > 2000 && mq135Value > 2500) {
    overallStatus = "FIRE / HEAVY SMOKE POSSIBLE";
  }
  else if (mq135Value > 2500) {
    overallStatus = "CHEMICAL / VOC DETECTED";
  }
  else if (mq9Value > 2200) {
    overallStatus = "FLAMMABLE GAS DETECTED";
  }
  else {
    overallStatus = "ENVIRONMENT NORMAL";
  }
}

// =====================================================
// WEBPAGE
// =====================================================

void handleRoot() {

  String page = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<title>ESP32 GAS MONITOR</title>

<meta name="viewport" content="width=device-width, initial-scale=1">

<meta http-equiv="refresh" content="2">

<style>

body{
  background:#111;
  color:white;
  font-family:Arial;
  text-align:center;
  padding:20px;
}

.card{
  background:#1e1e1e;
  padding:20px;
  margin:15px;
  border-radius:20px;
  box-shadow:0px 0px 10px #000;
}

.value{
  font-size:35px;
  font-weight:bold;
}

.status{
  font-size:22px;
  margin-top:10px;
}

h1{
  color:cyan;
}

.overall{
  background:red;
  padding:20px;
  border-radius:20px;
  font-size:25px;
  font-weight:bold;
}

</style>

</head>

<body>

<h1>ESP32 SMART GAS MONITOR</h1>

<div class="overall">
OVERALL STATUS:<br>
)rawliteral";

  page += overallStatus;

  page += R"rawliteral(
</div>

<div class="card">

<h2>MQ2 SENSOR</h2>

<div class="value">
)rawliteral";

  page += String(mq2Value);

  page += R"rawliteral(
</div>

<div class="status">
)rawliteral";

  page += mq2Status;

  page += R"rawliteral(
</div>

</div>

<div class="card">

<h2>MQ9 SENSOR</h2>

<div class="value">
)rawliteral";

  page += String(mq9Value);

  page += R"rawliteral(
</div>

<div class="status">
)rawliteral";

  page += mq9Status;

  page += R"rawliteral(
</div>

</div>

<div class="card">

<h2>MQ135 SENSOR</h2>

<div class="value">
)rawliteral";

  page += String(mq135Value);

  page += R"rawliteral(
</div>

<div class="status">
)rawliteral";

  page += mq135Status;

  page += R"rawliteral(
</div>

</div>

</body>
</html>

)rawliteral";

  server.send(200, "text/html", page);
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  analogSetAttenuation(ADC_11db);

  // ==========================================
  // WIFI
  // ==========================================

  WiFi.begin(ssid, password);

  Serial.println();
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // ==========================================
  // WEB SERVER
  // ==========================================

  server.on("/", handleRoot);

  server.begin();

  Serial.println("Web Server Started");

  Serial.println();
  Serial.println("Warming up sensors...");
  delay(5000);
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  server.handleClient();

  // ==========================================
  // READ SENSORS
  // ==========================================

  mq2Value = readAverage(MQ2_PIN);

  mq9Value = readAverage(MQ9_PIN);

  mq135Value = readAverage(MQ135_PIN);

  // ==========================================
  // ANALYZE
  // ==========================================

  analyzeSensors();

  // ==========================================
  // SERIAL MONITOR
  // ==========================================

  Serial.println("=================================");

  Serial.print("MQ2   : ");
  Serial.print(mq2Value);
  Serial.print(" ---> ");
  Serial.println(mq2Status);

  Serial.print("MQ9   : ");
  Serial.print(mq9Value);
  Serial.print(" ---> ");
  Serial.println(mq9Status);

  Serial.print("MQ135 : ");
  Serial.print(mq135Value);
  Serial.print(" ---> ");
  Serial.println(mq135Status);

  Serial.println();

  Serial.print("OVERALL STATUS: ");
  Serial.println(overallStatus);

  Serial.println("=================================");

  delay(1000);
}