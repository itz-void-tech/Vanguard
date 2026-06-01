/*
========================================================
ESP32 SD CARD FILE EXPLORER (STANDALONE TEST)
========================================================
This code creates a standalone web server to test your SD card.
It lists files, allows downloading, viewing, and deleting.

WIFI:
SSID     : sim
PASSWORD : simple12

CONNECTIONS (Standard VSPI):
MOSI -> GPIO 23
MISO -> GPIO 19
SCK  -> GPIO 18
CS   -> GPIO 5
========================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// WiFi Configuration
const char* ssid = "sim";
const char* password = "simple12";

WebServer server(80);
bool sdMounted = false;

// HTML Interface
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 SD Card Explorer</title>
    <style>
        body { font-family: Arial; background-color: #121212; color: #ffffff; padding: 20px; }
        h1 { color: #00bcd4; }
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { border: 1px solid #333; padding: 12px; text-align: left; }
        th { background-color: #1e1e1e; }
        tr:nth-child(even) { background-color: #1a1a1a; }
        button { background-color: #00bcd4; border: none; color: white; padding: 8px 12px; cursor: pointer; border-radius: 5px; margin-right: 5px; }
        button.del { background-color: #f44336; }
        button:hover { opacity: 0.8; }
    </style>
</head>
<body>
    <h1>📂 ESP32 SD Card Explorer</h1>
    <table>
        <thead>
            <tr>
                <th>File Name</th>
                <th>Size (KB)</th>
                <th>Actions</th>
            </tr>
        </thead>
        <tbody id="fileTable">
            <tr><td colspan="3">Loading...</td></tr>
        </tbody>
    </table>

    <script>
        async function fetchFiles() {
            const res = await fetch('/api/files');
            const files = await res.json();
            let html = '';
            if(files.length === 0) {
                html = '<tr><td colspan="3">No files found on SD Card.</td></tr>';
            } else {
                files.forEach(f => {
                    const size = (f.size / 1024).toFixed(2);
                    html += `<tr>
                        <td>${f.name}</td>
                        <td>${size} KB</td>
                        <td>
                            <button onclick="window.open('/download?file=${f.name}')">Download</button>
                            <button onclick="window.open('/view?file=${f.name}')">View</button>
                            <button class="del" onclick="deleteFile('${f.name}')">Delete</button>
                        </td>
                    </tr>`;
                });
            }
            document.getElementById('fileTable').innerHTML = html;
        }

        async function deleteFile(name) {
            if(confirm('Delete ' + name + '?')) {
                await fetch('/delete?file=' + name + '&api=true');
                fetchFiles();
            }
        }

        window.onload = fetchFiles;
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  Serial.print("Initializing SD card...");
  if (SD.begin(5)) {
    Serial.println(" mounted successfully.");
    sdMounted = true;
  } else {
    Serial.println(" mounting FAILED!");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP Address: ");
  Serial.println(WiFi.localIP());

  // Web routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", webpage);
  });

  server.on("/api/files", HTTP_GET, []() {
    if (!sdMounted) { server.send(500, "application/json", "[]"); return; }
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

  server.on("/download", HTTP_GET, []() {
    if (!sdMounted) { server.send(500, "text/plain", "SD card not mounted"); return; }
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

  server.on("/view", HTTP_GET, []() {
    if (!sdMounted) { server.send(500, "text/plain", "SD card not mounted"); return; }
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

  server.on("/delete", HTTP_GET, []() {
    if (!sdMounted) { server.send(500, "text/plain", "SD card not mounted"); return; }
    String path = server.arg("file");
    if (!path.startsWith("/")) path = "/" + path;
    if (SD.exists(path)) {
      SD.remove(path);
      server.send(200, "text/plain", "DELETED");
    } else {
      server.send(404, "text/plain", "File Not Found");
    }
  });

  server.begin();
  Serial.println("SD Explorer Server started.");
}

void loop() {
  server.handleClient();
}
