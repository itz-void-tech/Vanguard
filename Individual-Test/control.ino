#include <WiFi.h>
#include <WebServer.h>

// ================= WIFI =================
const char* ssid = "sim";
const char* password = "simple12";

WebServer server(80);

// ================= BTS7960 PINS =================

// LEFT MOTOR
#define L_RPWM 14
#define L_LPWM 12

// RIGHT MOTOR
#define R_RPWM 27
#define R_LPWM 26

// PWM SETTINGS
#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

#define L_RPWM_CH 0
#define L_LPWM_CH 1
#define R_RPWM_CH 2
#define R_LPWM_CH 3

int motorSpeed = 180;

// ================= MOTOR FUNCTIONS =================

void stopMotors() {
  ledcWrite(L_RPWM_CH, 0);
  ledcWrite(L_LPWM_CH, 0);

  ledcWrite(R_RPWM_CH, 0);
  ledcWrite(R_LPWM_CH, 0);
}

void moveForward() {
  ledcWrite(L_RPWM_CH, motorSpeed);
  ledcWrite(L_LPWM_CH, 0);

  ledcWrite(R_RPWM_CH, motorSpeed);
  ledcWrite(R_LPWM_CH, 0);
}

void moveBackward() {
  ledcWrite(L_RPWM_CH, 0);
  ledcWrite(L_LPWM_CH, motorSpeed);

  ledcWrite(R_RPWM_CH, 0);
  ledcWrite(R_LPWM_CH, motorSpeed);
}

void turnLeft() {
  ledcWrite(L_RPWM_CH, 0);
  ledcWrite(L_LPWM_CH, motorSpeed);

  ledcWrite(R_RPWM_CH, motorSpeed);
  ledcWrite(R_LPWM_CH, 0);
}

void turnRight() {
  ledcWrite(L_RPWM_CH, motorSpeed);
  ledcWrite(L_LPWM_CH, 0);

  ledcWrite(R_RPWM_CH, 0);
  ledcWrite(R_LPWM_CH, motorSpeed);
}

// ================= HTML =================

String webpage = R"rawliteral(

<!DOCTYPE html>
<html>
<head>
<title>ESP32 TANK</title>

<meta name="viewport" content="width=device-width, initial-scale=1">

<style>

body{
background:#111;
color:white;
font-family:Arial;
text-align:center;
}

button{
width:120px;
height:120px;
font-size:30px;
margin:10px;
border:none;
border-radius:20px;
background:#00bcd4;
color:white;
}

.stop{
background:red;
}

.slider{
width:300px;
}

</style>
</head>

<body>

<h1>ESP32 TANK CONTROL</h1>

<div>
<button onclick="sendCmd('F')">⬆</button>
</div>

<div>
<button onclick="sendCmd('L')">⬅</button>

<button class="stop" onclick="sendCmd('S')">STOP</button>

<button onclick="sendCmd('R')">➡</button>
</div>

<div>
<button onclick="sendCmd('B')">⬇</button>
</div>

<br><br>

<h2>Speed</h2>

<input type="range" min="0" max="255" value="180"
class="slider"
onchange="setSpeed(this.value)">

<script>

function sendCmd(cmd){
fetch("/cmd?dir="+cmd);
}

function setSpeed(val){
fetch("/speed?val="+val);
}

</script>

</body>
</html>

)rawliteral";

// ================= SETUP =================

void setup() {

  Serial.begin(115200);

  // PWM Setup
  ledcSetup(L_RPWM_CH, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(L_LPWM_CH, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(R_RPWM_CH, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(R_LPWM_CH, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(L_RPWM, L_RPWM_CH);
  ledcAttachPin(L_LPWM, L_LPWM_CH);

  ledcAttachPin(R_RPWM, R_RPWM_CH);
  ledcAttachPin(R_LPWM, R_LPWM_CH);

  stopMotors();

  // WIFI
  WiFi.begin(ssid, password);

  Serial.print("Connecting");

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ================= ROUTES =================

  server.on("/", [](){
    server.send(200, "text/html", webpage);
  });

  server.on("/cmd", [](){

    String dir = server.arg("dir");

    if(dir=="F") moveForward();
    else if(dir=="B") moveBackward();
    else if(dir=="L") turnLeft();
    else if(dir=="R") turnRight();
    else if(dir=="S") stopMotors();

    server.send(200, "text/plain", "OK");
  });

  server.on("/speed", [](){

    motorSpeed = server.arg("val").toInt();

    server.send(200, "text/plain", "Speed Updated");
  });

  server.begin();

  Serial.println("Web server started");
}

// ================= LOOP =================

void loop() {

  server.handleClient();

}