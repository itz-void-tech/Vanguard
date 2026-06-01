/*
   ================================================================================
   ESP32 ROVER - SYSTEM 3 (FIRE FIGHTING, GPS & ENV SENSORS)
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
   DEVICE CONFIGURATION (ESP3 SPECIFICS):
   - Flame Sensors:
     - FLAME1 -> GPIO 34 (Digital)
     - FLAME2 -> GPIO 35 (Digital)
     - FLAME3 -> GPIO 32 (Digital)
   - Pump/Sprayer Relay:
     - Pin -> GPIO 27 (Digital Output)
   - Sweeping Servo:
     - Pin -> GPIO 13 (PWM)
   - NEO-6M GPS Module:
     - GPS TX -> GPIO 16 (ESP32 RX2)
     - GPS RX -> GPIO 17 (ESP32 TX2)
   - DHT11 Sensor:
     - Pin -> GPIO 4 (Digital)
   - Gas Sensors (MQ2, MQ9, MQ135):
     - MQ2 -> GPIO 33
     - MQ9 -> GPIO 25
     - MQ135 -> GPIO 26

   MQTT TOPICS:
   - Subscribed Topics:
     - None (Publisher node)
   - Published Topics:
     - "rover/sensors/dht"    (Payload: {"temp":XX.X,"hum":YY.Y})
     - "rover/sensors/gps"    (Payload: {"lat":XX.XXXX,"lon":YY.YYYY,"speed":S.S,"sat":N,"valid":bool})
     - "rover/sensors/fire"   (Payload: ON / OFF)
     - "rover/siren/cmd"     (Payload: "fire" / "stop" to sound ESP1's siren)

   ================================================================================
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <TinyGPSPlus.h>
#include <DHT.h>

// ==========================================
// WIFI & MQTT CONFIGURATION
// ==========================================
const char* ssid          = "sim";
const char* password      = "simple12";
const char* mqtt_server   = "broker.hivemq.com"; // Public Broker from your settings
const int mqtt_port       = 1883;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==========================================
// FLAME SENSOR & FIRE SUPPRESSION PINS
// ==========================================
#define FLAME1 34
#define FLAME2 35
#define FLAME3 32

#define RELAY_PIN 27
#define SERVO_PIN 13

Servo sprayServo;
bool wasFireActive = false;
bool wasGasSirenActive = false;

// ==========================================
// GPS NEO-6M HARDWARE SERIAL CONFIG
// ==========================================
TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // Use UART2
#define GPS_RX 16            // ESP32 RX <- GPS TX
#define GPS_TX 17            // ESP32 TX -> GPS RX

// ==========================================
// DHT11 CONFIGURATION
// ==========================================
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ==========================================
// GAS SENSORS CONFIGURATION
// ==========================================
#define MQ2_PIN    33
#define MQ9_PIN    25
#define MQ135_PIN  26

float mq2Value = 0;
float mq9Value = 0;
float mq135Value = 0;

String mq2Status = "";
String mq9Status = "";
String mq135Status = "";
String overallStatus = "";

// Timer variables for periodic sensor publishing
unsigned long lastPublishTime = 0;

// ==========================================
// CUSTOM NON-BLOCKING DELAY FOR GPS & MQTT
// ==========================================
// This processes serial GPS sentences and MQTT keepalives during delay loops
void gpsDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    // Process incoming GPS bytes
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }
    // Process MQTT loop
    if (mqttClient.connected()) {
      mqttClient.loop();
    }
    delay(1);
  } while (millis() - start < ms);
}

// ==========================================
// GAS SENSOR FUNCTIONS
// ==========================================
float readAverage(int pin) {
  long total = 0;
  for (int i = 0; i < 20; i++) {
    total += analogRead(pin);
    gpsDelay(5);
  }
  return total / 20.0;
}

void analyzeSensors() {
  if (mq2Value < 1200) mq2Status = "SAFE";
  else if (mq2Value < 2000) mq2Status = "WARNING";
  else mq2Status = "DANGEROUS SMOKE/GAS";

  if (mq9Value < 1500) mq9Status = "SAFE";
  else if (mq9Value < 2200) mq9Status = "GAS DETECTED";
  else mq9Status = "HIGH FLAMMABLE GAS";

  if (mq135Value < 1500) mq135Status = "AIR QUALITY GOOD";
  else if (mq135Value < 2500) mq135Status = "POLLUTED AIR";
  else mq135Status = "HEAVY SMOKE / VOC";

  if (mq2Value > 2000 && mq135Value > 2500) overallStatus = "FIRE / HEAVY SMOKE POSSIBLE";
  else if (mq135Value > 2500) overallStatus = "CHEMICAL / VOC DETECTED";
  else if (mq9Value > 2200) overallStatus = "FLAMMABLE GAS DETECTED";
  else overallStatus = "ENVIRONMENT NORMAL";
}

// ==========================================
// FIRE FIGHTING FUNCTIONS
// ==========================================
bool isFireDetected() {
  // Sensors go LOW when infrared/flame is detected
  bool fire1 = digitalRead(FLAME1) == LOW;
  bool fire2 = digitalRead(FLAME2) == LOW;
  bool fire3 = digitalRead(FLAME3) == LOW;
  return (fire1 || fire2 || fire3);
}

void stopSystem() {
  digitalWrite(RELAY_PIN, LOW);
  sprayServo.write(90); // Center the nozzle
  
  if (wasFireActive) {
    Serial.println("FIRE EXTINGUISHED / NO DANGER");
    if (mqttClient.connected()) {
      mqttClient.publish("rover/sensors/fire", "OFF", true);
      mqttClient.publish("rover/siren/cmd", "stop", true);
    }
    wasFireActive = false;
  }
}

void runFireFighting() {
  if (isFireDetected()) {
    if (!wasFireActive) {
      Serial.println("CRITICAL: FIRE DETECTED!");
      if (mqttClient.connected()) {
        mqttClient.publish("rover/sensors/fire", "ON", true);
        // Command ESP1 to trigger the fire siren
        mqttClient.publish("rover/siren/cmd", "fire", true);
      }
      wasFireActive = true;
    }

    // Turn on the water pump relay
    digitalWrite(RELAY_PIN, HIGH);

    // Sweep Left -> Right
    for (int pos = 0; pos <= 180; pos += 2) {
      sprayServo.write(pos);
      gpsDelay(15); // Custom delay feeds GPS buffer
      
      if (!isFireDetected()) {
        stopSystem();
        return;
      }
    }

    // Sweep Right -> Left
    for (int pos = 180; pos >= 0; pos -= 2) {
      sprayServo.write(pos);
      gpsDelay(15);
      
      if (!isFireDetected()) {
        stopSystem();
        return;
      }
    }
  } else {
    stopSystem();
  }
}

// ==========================================
// PUBLISH SENSOR TELEMETRY OVER MQTT
// ==========================================
void publishSensors() {
  unsigned long now = millis();
  // Read and publish every 5 seconds
  if (now - lastPublishTime >= 5000 || lastPublishTime == 0) {
    lastPublishTime = now;

    // 1. Read DHT11 Temperature & Humidity
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      String dhtJson = "{\"temp\":" + String(t, 1) + ",\"hum\":" + String(h, 1) + "}";
      if (mqttClient.connected()) {
        mqttClient.publish("rover/sensors/dht", dhtJson.c_str(), true);
      }
      Serial.println("Published DHT data: " + dhtJson);
    } else {
      Serial.println("DHT11 read failure!");
    }

    // 2. Read TinyGPS++ Variables
    // Feed the parser with anything waiting in buffer
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }

    bool validFix = gps.location.isValid();
    double latVal = validFix ? gps.location.lat() : 0.0;
    double lonVal = validFix ? gps.location.lng() : 0.0;
    double speedVal = validFix ? gps.speed.kmph() : 0.0;
    int satVal = validFix ? gps.satellites.value() : 0;

    String gpsJson = "{\"lat\":" + String(latVal, 6) + 
                     ",\"lon\":" + String(lonVal, 6) + 
                     ",\"speed\":" + String(speedVal, 1) + 
                     ",\"sat\":" + String(satVal) + 
                     ",\"valid\":" + String(validFix ? "true" : "false") + "}";

    if (mqttClient.connected()) {
      mqttClient.publish("rover/sensors/gps", gpsJson.c_str(), true);
    }
    Serial.println("Published GPS data: " + gpsJson);

    // 3. Read and Publish Gas Data
    mq2Value = readAverage(MQ2_PIN);
    mq9Value = readAverage(MQ9_PIN);
    mq135Value = readAverage(MQ135_PIN);
    analyzeSensors();

    String gasJson = "{\"mq2\":{\"val\":" + String(mq2Value, 0) + ",\"stat\":\"" + mq2Status + "\"}," +
                     "\"mq9\":{\"val\":" + String(mq9Value, 0) + ",\"stat\":\"" + mq9Status + "\"}," +
                     "\"mq135\":{\"val\":" + String(mq135Value, 0) + ",\"stat\":\"" + mq135Status + "\"}," +
                     "\"overall\":\"" + overallStatus + "\"}";
                     
    if (mqttClient.connected()) {
      mqttClient.publish("rover/sensors/gas", gasJson.c_str(), true);
      
      // Auto-trigger gas siren warning on ESP1 if gas levels rise to dangerous range
      bool isGasDangerous = (mq2Value > 550) || (mq9Value > 2200) || (mq135Value > 3300);
      if (isGasDangerous) {
        if (!wasGasSirenActive) {
          mqttClient.publish("rover/siren/cmd", "gas", true);
          wasGasSirenActive = true;
        }
      } else {
        if (wasGasSirenActive) {
          mqttClient.publish("rover/siren/cmd", "stop", true);
          wasGasSirenActive = false;
        }
      }
    }
    Serial.println("Published Gas data: " + gasJson);
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
      
      String clientId = "ESP32_Rover_3_Sensors_" + String(random(0xffff), HEX);
      if (mqttClient.connect(clientId.c_str())) {
        Serial.println(" Connected!");
        mqttClient.publish("rover/sensors/fire", wasFireActive ? "ON" : "OFF", true);
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
  analogSetAttenuation(ADC_11db);

  // Initialize sensors & relay pins
  pinMode(FLAME1, INPUT);
  pinMode(FLAME2, INPUT);
  pinMode(FLAME3, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relay off initially

  // Setup sweeping servo
  sprayServo.setPeriodHertz(50);
  sprayServo.attach(SERVO_PIN, 500, 2400);
  sprayServo.write(90); // Center position

  // Initialize DHT11 sensor
  dht.begin();

  // Initialize GPS Serial port (9600 baud, UART2)
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

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

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection failed! Working in Offline / Local AP mode...");
  }

  // Setup MQTT client
  mqttClient.setServer(mqtt_server, mqtt_port);
  
  Serial.println("==================================");
  Serial.println("ESP32 FRONT SCOUT & SENSORS READY");
  Serial.println("==================================");
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  // Feed GPS parser
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  
  handleMqtt();
  runFireFighting();
  publishSensors();
}
