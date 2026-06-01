/*
   ================================================================================
   ESP32 ROVER - SYSTEM 4 (ROBOTIC ARM CONTROL - OPTIONAL STANDALONE)
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
   OPTIONAL / STANDALONE CONFIGURATION (IF RUNNING ROBOTIC ARM AS ESP4):
   - Robotic Arm Servos:
     - Arm Base Servo   -> GPIO 13
     - Arm Shoulder     -> GPIO 12
     - Arm Elbow        -> GPIO 14
     - Arm Gripper      -> GPIO 27

   ================================================================================
*/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

// ================= AP SETTINGS =================
const char* ssid = "RobotArm";
const char* password = "12345678";

// ================= SERVOS =================
Servo servoBase;
Servo servoShoulder;
Servo servoElbow;
Servo servoGripper;

// ================= PINS =================
#define BASE_PIN      13
#define SHOULDER_PIN  12
#define ELBOW_PIN     14
#define GRIPPER_PIN   27

// ================= SERVER =================
AsyncWebServer server(80);

// ================= POSITIONS =================
int basePos = 90;
int shoulderPos = 90;
int elbowPos = 90;
int gripperPos = 90;

// ================= HTML PAGE =================
const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>ESP32 Robot Arm</title>

<style>

body{
    background:#111;
    color:white;
    font-family:Arial;
    margin:0;
    padding:20px;
}

.container{
    max-width:500px;
    margin:auto;
}

.card{
    background:#1b1b1b;
    padding:20px;
    border-radius:20px;
    margin-bottom:20px;
    box-shadow:0 0 20px rgba(0,0,0,0.5);
}

h1{
    text-align:center;
    color:#00ff99;
}

.slider-container{
    margin-top:25px;
}

.label{
    font-size:20px;
    margin-bottom:10px;
}

.value{
    color:#00ff99;
    font-weight:bold;
}

.slider{
    width:100%;
    height:20px;
}

.status{
    text-align:center;
    margin-top:20px;
    color:#00ff99;
}

</style>

</head>

<body>

<div class="container">

<h1>ESP32 Robotic Arm</h1>

<div class="card">

<div class="slider-container">
<div class="label">
Base:
<span class="value" id="baseVal">90</span>
</div>

<input type="range"
min="0"
max="180"
value="90"
class="slider"
id="baseSlider">
</div>

<div class="slider-container">
<div class="label">
Shoulder:
<span class="value" id="shoulderVal">90</span>
</div>

<input type="range"
min="0"
max="180"
value="90"
class="slider"
id="shoulderSlider">
</div>

<div class="slider-container">
<div class="label">
Elbow:
<span class="value" id="elbowVal">90</span>
</div>

<input type="range"
min="0"
max="180"
value="90"
class="slider"
id="elbowSlider">
</div>

<div class="slider-container">
<div class="label">
Gripper:
<span class="value" id="gripperVal">90</span>
</div>

<input type="range"
min="0"
max="180"
value="90"
class="slider"
id="gripperSlider">
</div>

<div class="status">
Connected to ESP32 Robot Arm
</div>

</div>

</div>

<script>

function sendServo(name, value){

    fetch(`/set?servo=${name}&value=${value}`);
}

// ================= BASE =================
const baseSlider =
document.getElementById("baseSlider");

baseSlider.oninput = function(){

    document.getElementById("baseVal")
    .innerText = this.value;

    sendServo("base", this.value);
}

// ================= SHOULDER =================
const shoulderSlider =
document.getElementById("shoulderSlider");

shoulderSlider.oninput = function(){

    document.getElementById("shoulderVal")
    .innerText = this.value;

    sendServo("shoulder", this.value);
}

// ================= ELBOW =================
const elbowSlider =
document.getElementById("elbowSlider");

elbowSlider.oninput = function(){

    document.getElementById("elbowVal")
    .innerText = this.value;

    sendServo("elbow", this.value);
}

// ================= GRIPPER =================
const gripperSlider =
document.getElementById("gripperSlider");

gripperSlider.oninput = function(){

    document.getElementById("gripperVal")
    .innerText = this.value;

    sendServo("gripper", this.value);
}

</script>

</body>
</html>

)rawliteral";

// ================= SETUP =================
void setup() {

    Serial.begin(115200);

    // ================= SERVO INIT =================
    servoBase.attach(BASE_PIN);
    servoShoulder.attach(SHOULDER_PIN);
    servoElbow.attach(ELBOW_PIN);
    servoGripper.attach(GRIPPER_PIN);

    servoBase.write(basePos);
    servoShoulder.write(shoulderPos);
    servoElbow.write(elbowPos);
    servoGripper.write(gripperPos);

    // ================= ACCESS POINT =================
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();

    Serial.println();
    Serial.println("===============================");
    Serial.println("ESP32 ROBOT ARM AP STARTED");
    Serial.println("===============================");

    Serial.print("AP IP Address: ");
    Serial.println(IP);

    // ================= WEB PAGE =================
    server.on("/", HTTP_GET,
    [](AsyncWebServerRequest *request){

        request->send_P(200,
                        "text/html",
                        index_html);
    });

    // ================= SERVO CONTROL =================
    server.on("/set", HTTP_GET,
    [](AsyncWebServerRequest *request){

        if(request->hasParam("servo") &&
           request->hasParam("value")){

            String servo =
            request->getParam("servo")->value();

            int value =
            request->getParam("value")
            ->value().toInt();

            value = constrain(value, 0, 180);

            // ================= BASE =================
            if(servo == "base"){

                basePos = value;
                servoBase.write(basePos);
            }

            // ================= SHOULDER =================
            else if(servo == "shoulder"){

                shoulderPos = value;
                servoShoulder.write(shoulderPos);
            }

            // ================= ELBOW =================
            else if(servo == "elbow"){

                elbowPos = value;
                servoElbow.write(elbowPos);
            }

            // ================= GRIPPER =================
            else if(servo == "gripper"){

                gripperPos = value;
                servoGripper.write(gripperPos);
            }

            Serial.print(servo);
            Serial.print(" -> ");
            Serial.println(value);

            request->send(200,
                          "text/plain",
                          "OK");
        }
    });

    // ================= START SERVER =================
    server.begin();

    Serial.println("Web Server Started");
}

void loop() {

}
