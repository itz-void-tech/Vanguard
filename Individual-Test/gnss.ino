/*
   ESP32 + NEO-6M/NEO-7M GPS LIVE LOCATION MAP
   ------------------------------------------
   Features:
   - ESP32 WiFi Station Mode
   - Connects to your WiFi
   - Live GPS tracking on Leaflet Map
   - Auto-refreshing marker
   - Shows latitude, longitude, satellites, speed
   - Mobile friendly UI

   WiFi:
   SSID     : sim
   Password : simple12
*/

#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPSPlus.h>

// ================= WIFI =================
const char* ssid = "sim";
const char* password = "simple12";

// ================= GPS =================
TinyGPSPlus gps;

// ESP32 UART2
HardwareSerial gpsSerial(2);

// GPS PINS
#define GPS_RX 16   // ESP32 RX <- GPS TX
#define GPS_TX 17   // ESP32 TX -> GPS RX

// ================= SERVER =================
WebServer server(80);

// ================= VARIABLES =================
double latitude = 0.0;
double longitude = 0.0;
double speedKmph = 0.0;
int satellites = 0;
bool gpsValid = false;

// ================= HTML PAGE =================
String webpage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>ESP32 Live GPS Tracker</title>

<link rel="stylesheet" href="https://unpkg.com/leaflet/dist/leaflet.css"/>

<style>
    body{
        margin:0;
        padding:0;
        font-family:Arial;
        background:#0f172a;
        color:white;
    }

    #map{
        height:75vh;
        width:100%;
    }

    .top{
        padding:15px;
        background:#111827;
    }

    .title{
        font-size:24px;
        font-weight:bold;
        color:#38bdf8;
    }

    .info{
        margin-top:10px;
        line-height:1.8;
        font-size:16px;
    }

    .status{
        color:#22c55e;
        font-weight:bold;
    }

    .waiting{
        color:#ef4444;
        font-weight:bold;
    }

    .card{
        background:#1e293b;
        padding:12px;
        border-radius:12px;
        margin-top:10px;
    }
</style>
</head>

<body>

<div class="top">
    <div class="title">📍 ESP32 LIVE GPS TRACKER</div>

    <div class="card">
        <div id="status" class="waiting">Waiting for GPS...</div>

        <div class="info">
            🌍 Latitude: <span id="lat">0</span><br>
            🌍 Longitude: <span id="lon">0</span><br>
            🛰 Satellites: <span id="sat">0</span><br>
            🚗 Speed: <span id="speed">0</span> km/h
        </div>
    </div>
</div>

<div id="map"></div>

<script src="https://unpkg.com/leaflet/dist/leaflet.js"></script>

<script>

var map = L.map('map').setView([20.5937, 78.9629], 5);

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution:'© OpenStreetMap'
}).addTo(map);

var marker = L.marker([20.5937, 78.9629]).addTo(map);

async function updateGPS(){

    try{

        let response = await fetch('/gps');
        let data = await response.json();

        document.getElementById("lat").innerHTML = data.lat;
        document.getElementById("lon").innerHTML = data.lon;
        document.getElementById("sat").innerHTML = data.sat;
        document.getElementById("speed").innerHTML = data.speed;

        if(data.valid){

            document.getElementById("status").innerHTML = "✅ GPS Connected";
            document.getElementById("status").className = "status";

            marker.setLatLng([data.lat, data.lon]);

            map.setView([data.lat, data.lon], 18);

        }else{

            document.getElementById("status").innerHTML = "❌ Waiting for GPS Signal";
            document.getElementById("status").className = "waiting";
        }

    }catch(err){
        console.log(err);
    }
}

setInterval(updateGPS, 2000);

</script>

</body>
</html>
)rawliteral";

// ================= HANDLE ROOT =================
void handleRoot() {
  server.send(200, "text/html", webpage);
}

// ================= HANDLE GPS =================
void handleGPS() {

  String json = "{";

  json += "\"lat\":" + String(latitude, 6) + ",";
  json += "\"lon\":" + String(longitude, 6) + ",";
  json += "\"speed\":" + String(speedKmph, 2) + ",";
  json += "\"sat\":" + String(satellites) + ",";
  json += "\"valid\":" + String(gpsValid ? "true" : "false");

  json += "}";

  server.send(200, "application/json", json);
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);

  // GPS SERIAL
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  // WIFI CONNECT
  WiFi.begin(ssid, password);

  Serial.println();
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // SERVER ROUTES
  server.on("/", handleRoot);
  server.on("/gps", handleGPS);

  server.begin();

  Serial.println("Web Server Started");
}

// ================= LOOP =================
void loop() {

  // READ GPS DATA
  while (gpsSerial.available() > 0) {

    char c = gpsSerial.read();

    gps.encode(c);
  }

  // UPDATE GPS VALUES
  if (gps.location.isUpdated()) {

    latitude = gps.location.lat();
    longitude = gps.location.lng();

    speedKmph = gps.speed.kmph();

    satellites = gps.satellites.value();

    gpsValid = true;

    Serial.println("========== GPS DATA ==========");
    Serial.print("Latitude: ");
    Serial.println(latitude, 6);

    Serial.print("Longitude: ");
    Serial.println(longitude, 6);

    Serial.print("Speed: ");
    Serial.println(speedKmph);

    Serial.print("Satellites: ");
    Serial.println(satellites);
  }

  // CHECK GPS TIMEOUT
  if (millis() > 10000 && gps.charsProcessed() < 10) {
    Serial.println("No GPS detected!");
    gpsValid = false;
  }

  server.handleClient();
}