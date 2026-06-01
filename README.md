# 🌌 Autonomous Disaster Scouting & Rescue Rover (Project Zyro)

<div align="center">
  
  ![Rover Concept Banner](images/disaster_rover_concept.png)
  
  [![Hackathon](https://img.shields.io/badge/Hackathon-ZYRO%202026-blueviolet?style=for-the-badge&logo=eventbrite)](https://github.com/)
  [![Award](https://img.shields.io/badge/Award-🥇%20Best%20Beginners%20Team-gold?style=for-the-badge&logo=award)](https://github.com/)
  [![Host](https://img.shields.io/badge/Host-Kalyani%20Government%20Engineering%20College-orange?style=for-the-badge&logo=academia)](https://kgec.edu.in/)
  [![Platform](https://img.shields.io/badge/Platform-ESP32%20|%20C++%20|%20MQTT-brightgreen?style=for-the-badge&logo=espressif)](https://www.espressif.com/en/products/socs/esp32)

  <p align="center">
    <strong>An IoT-enabled distributed robotic ecosystem designed for real-time hazard identification, atmospheric profiling, GPS mapping, active firefighting, and survivors searching.</strong>
  </p>

  <h4>
    👉 <a href="#-system-architecture">Architecture</a> 
    • <a href="#-esp32-subsystem-breakdown">ESP32 Nodes</a> 
    • <a href="#-ground-control-station">GCS Dashboard</a> 
    • <a href="#-telegram-telemetry-vault">Telegram Bot</a> 
    • <a href="#-hardware-pin-mappings">Pinouts</a> 
    • <a href="#-getting-started">Getting Started</a>
  </h4>

  <sub>Built by the **Disaster Techies** for ZYRO Hackathon organised by Kalyani Government Engineering College.</sub>
</div>

---

## 📖 Project Overview

When disaster strikes—be it industrial gas leaks, chemical fires, or structural collapses—sending human responders inside is highly hazardous. This **Disaster Scouting & Rescue Rover** is a smart, distributed robotic platform built to scout hazard zones, transmit real-time telemetry, and autonomously suppress hazards. 

The rover's electronic brain consists of a **three-node ESP32 cluster** cooperating asynchronously over a public MQTT network. Together with an interactive Cyberpunk-themed Web Ground Control Station (GCS) and a secure Telegram Bot interface, the system provides live environmental analytics, automatic warning sirens, and cloud blackbox logging.

### 🌟 Key Highlights
*   🏆 **Award Winner**: Evaluated and awarded **"Best Beginners Team"** at the prestigious **ZYRO Hackathon** (Kalyani Government Engineering College).
*   🌐 **Multi-Core Distributed Architecture**: Workload is balanced across 3 independent ESP32 microcontrollers communicating via MQTT.
*   🚒 **Autonomous Fire Suppression**: Integrated flame sensors command a sweeping servo water nozzle and high-volume spray pump.
*   ☣️ **Toxic Gas Signature Detection**: Reads and classifies data from MQ2, MQ9, and MQ135 sensors to flag hazardous air quality.
*   🛰️ **Real-Time GPS Mapping**: Streamlined GNSS tracking using a NEO-6M module for instant coordinate mapping on Google Maps.
*   📱 **Dual Remote Portals**: A sleek local Web GCS Dashboard for manual operations and a non-blocking Universal Telegram Bot with automated danger notifications.

---

## 🏗️ System Architecture

The robot splits its operational payload among three ESP32 microcontrollers to ensure non-blocking execution of timing-sensitive tasks (like servo sweeps, motor PWM, GPS parsing, and SD card file writing).

```mermaid
graph TD
    %% MQTT Broker
    Broker[HiveMQ MQTT Broker] style Broker fill:#8b5cf6,stroke:#fff,stroke-width:2px,color:#fff;
    
    %% ESP Node 1
    subgraph ESP1 [ESP32 - Control & Actuation]
        direction TB
        E1[Core Loop]
        Motor[BTS7960 Motor Driver]
        Arm[4-Axis Robotic Arm]
        PanTilt[2-Axis Camera Gimbal]
        Siren[Passive Buzzer Alert]
        Web1[Local Control WebServer]
        US[Ultrasonic Collision Guard]
    end
    style ESP1 fill:#1e1b4b,stroke:#06b6d4,stroke-width:1px,color:#fff;

    %% ESP Node 2
    subgraph ESP2 [ESP32 - Blackbox & Gateway]
        direction TB
        E2[Core Loop]
        SDCard[VSPI SD Reader]
        TeleBot[Universal Telegram Bot]
        Firebase[Firebase Cloud DB]
        Web2[SD File Server]
    end
    style ESP2 fill:#180f2b,stroke:#f59e0b,stroke-width:1px,color:#fff;

    %% ESP Node 3
    subgraph ESP3 [ESP32 - Front Scout & Firefighter]
        direction TB
        E3[Core Loop]
        Flame[3x Flame Sensors]
        Relay[Extinguisher Pump Relay]
        Sweep[Servo Sweeper Nozzle]
        NEO6[NEO-6M GPS Module]
        DHT[DHT11 Temp/Hum]
        GasSensors[MQ2, MQ9, MQ135 Gas Payload]
    end
    style ESP3 fill:#1c1917,stroke:#ef4444,stroke-width:1px,color:#fff;

    %% Connections
    ESP3 -- Telemetry & Fire Status --> Broker
    Broker -- Commands/Speed/Angles --> ESP1
    Broker -- Raw Telemetry Feed --> ESP2
    ESP2 -- Alerts / Geo-location --> TeleBot
    ESP2 -- Alert Backups --> Firebase
    
    classDef default fill:#111827,stroke:#374151,color:#f3f4f6;
```

---

## 📟 ESP32 Subsystem Breakdown

### 1. ESP1: Drive Control & Sirens Node
Acts as the physical driver of the rover. It receives driving commands, regulates speeds, controls the pan-tilt gimbal, and commands the mechanical arm joints.
*   **Motor Drive**: Controls high-power **BTS7960 H-Bridge Drivers** via high-frequency ESP32 LEDC PWM.
*   **Robotic Arm & Gimbal**: Coordinates 6 servos (4-axis arm joints + 2-axis camera scanner).
*   **Ultrasonic Safety Interlocks**: Auto-halts/reverses standard driving if front or rear ultrasonic sensors detect obstacles.
*   **Multi-Tone Acoustic Siren**: Plays distinct audio frequencies for various disaster triggers (Gas leaks, Fire, Low Battery, SOS Morse Code).
*   **Local Web Server**: Hosts a styling-optimized web portal for direct offline manual drive and siren triggering.
*   **Code Location**: [1esp.ino](ESP32_codes/1esp.ino)

### 2. ESP2: Blackbox Logger & Gateway Node
Acts as the central telemetry repository, alert center, and internet gateway.
*   **SD File Vault**: Records telemetry rows in CSV format with automatic header initialization.
*   **SD File Server**: Integrates a file manager web GUI allowing users to view, download, or delete logs directly from a browser.
*   **Telegram Command Bot**: Connects to the Universal Telegram Bot API to respond to user commands (`/status`, `/location`, `/stop`, `/fire_siren`, `/mute`, `/toggle_live`).
*   **Critical Danger Alerts**: Monitors telemetry and sends instant danger messages to Telegram if environmental limits are violated.
*   **Cloud Synchronizer**: Interacts with the Firebase API to push alerts to a Firebase Realtime Database.
*   **Code Location**: [2esp.ino](ESP32_codes/2esp.ino)

### 3. ESP3: Front Scout & Firefighter Node
Acts as the eyes and environmental analysis package of the rover.
*   **Active Firefighting**: Continuously scans 3 flame sensors. If a fire triggers, it energizes a water pump relay and sweeps a servo-mounted water nozzle back and forth from 0° to 180°.
*   **Atmospheric Sensors**: Monitors real-time ambient temperature and humidity using a DHT11.
*   **Toxic Gas Signature**: Analyzes analog values from MQ2, MQ9, MQ135 to classify the current hazard state (e.g., Toxic Gas, Flammable Leak, Chemical VOC).
*   **NEO-6M GPS Tracker**: Runs TinyGPS++ decoding on hardware UART2 to publish geo-location statistics.
*   **Code Location**: [3esp.ino](ESP32_codes/3esp.ino)

---

## 📊 MQTT Communication Scheme

The distributed nodes coordinate asynchronously using standard MQTT topics.

| Topic | Publisher | Subscriber(s) | Payload Example / Description |
| :--- | :---: | :---: | :--- |
| `rover/control/cmd` | Telegram Bot, Web GCS | **ESP1** | `F` (Forward), `B` (Backward), `L` (Left), `R` (Right), `S` (Stop) |
| `rover/control/speed` | Web GCS | **ESP1** | `0` to `255` (LEDC PWM Duty Cycle) |
| `rover/siren/cmd` | ESP3, Telegram Bot, GCS | **ESP1** | `gas`, `human`, `fire`, `battery`, `sos`, `auto`, `obstacle`, `stop` |
| `rover/arm/cmd` | Web GCS | **ESP1** | `{"base":90, "shoulder":45, "elbow":60, "gripper":30}` (Servos) |
| `rover/pantilt/cmd` | Web GCS | **ESP1** | `{"pan":90, "tilt":45}` (Camera Gimbal) |
| `rover/sensors/dht` | **ESP3** | ESP2, Web GCS | `{"temp":28.5,"hum":62.4}` |
| `rover/sensors/gps` | **ESP3** | ESP2, Web GCS | `{"lat":22.989,"lon":88.453,"speed":0.2,"sat":6,"valid":true}` |
| `rover/sensors/gas` | **ESP3** | ESP2, Web GCS | `{"mq2":{"val":320,"stat":"SAFE"},"mq9":{"val":450,"stat":"SAFE"},...}` |
| `rover/sensors/fire` | **ESP3** | ESP2, Web GCS | `ON` or `OFF` |
| `rover/control/status` | **ESP1** | ESP2, Web GCS | `MOTOR:FORWARD \| SPEED:180` |

---

## 🛠️ Hardware Pin Mappings

### Node 1: Control & Siren (ESP32)
| Component | Device Pin | ESP32 GPIO | Description |
| :--- | :--- | :---: | :--- |
| **BTS7960 Motor Driver** | Left RPWM | **GPIO 12** | Left Motor Forward (PWM Channel 1) |
| | Left LPWM | **GPIO 14** | Left Motor Reverse (PWM Channel 0) |
| | Right RPWM | **GPIO 26** | Right Motor Forward (PWM Channel 3) |
| | Right LPWM | **GPIO 27** | Right Motor Reverse (PWM Channel 2) |
| **Passive Buzzer** | I/O Signal | **GPIO 25** | Play sirens & alerts (PWM Channel 4) |
| **Gimbal Gimbal** | Pan Servo | **GPIO 23** | Camera Pan Joint |
| | Tilt Servo | **GPIO 22** | Camera Tilt Joint |
| **Robotic Arm** | Base Servo | **GPIO 21** | Arm Base |
| | Shoulder Servo| **GPIO 19** | Arm Shoulder Joint |
| | Elbow Servo | **GPIO 18** | Arm Elbow Joint |
| | Gripper Servo | **GPIO 5** | Arm Gripper Claw |
| **Front Ultrasonic** | Trigger / Echo | **GPIO 4 / GPIO 2** | Collision-Avoidance Front |
| **Back Ultrasonic** | Trigger / Echo | **GPIO 15 / GPIO 13**| Collision-Avoidance Back |

### Node 2: Logger & Gateway (ESP32)
| Component | Device Pin | ESP32 GPIO | Description |
| :--- | :--- | :---: | :--- |
| **Micro SD Card Reader**| CS (Chip Select)| **GPIO 5** | SPI Chip Select |
| | SCK | **GPIO 18** | SPI Clock |
| | MISO | **GPIO 19** | SPI Master In Slave Out |
| | MOSI | **GPIO 23** | SPI Master Out Slave In |

### Node 3: Front Scout & Firefighter (ESP32)
| Component | Device Pin | ESP32 GPIO | Description |
| :--- | :--- | :---: | :--- |
| **Flame Detectors** | Flame 1 (Left) | **GPIO 34** | Digital Input (Low on fire) |
| | Flame 2 (Center)| **GPIO 35** | Digital Input (Low on fire) |
| | Flame 3 (Right) | **GPIO 32** | Digital Input (Low on fire) |
| **Extinguishing Pump**| Relay Trigger | **GPIO 27** | Digital Output (High = Spray On) |
| **Sweeping Servo** | SG90 Signal | **GPIO 13** | sweeps spray nozzle 0-180 degrees |
| **NEO-6M GPS** | GPS TX | **GPIO 16** | hardware Serial RX2 |
| | GPS RX | **GPIO 17** | hardware Serial TX2 |
| **Weather Monitor** | DHT11 Signal | **GPIO 4** | Digital Input |
| **MQ-2 Gas Sensor** | Analog out | **GPIO 33** | Smoke/LPG concentration |
| **MQ-9 Gas Sensor** | Analog out | **GPIO 25** | Flammable Gas / Carbon Monoxide |
| **MQ-135 Gas Sensor**| Analog out | **GPIO 26** | Air Quality index (VOCs) |

---

## 💻 Ground Control Station

The main control interface is a beautifully designed, cyberpunk slate-themed Ground Control Station (`dashboard.html`). It is fully responsive and communicates over standard web integrations.

### Visual Preview of the Cockpit UI
*(Here is the interactive HUD console structure designed in the project)*
```
+---------------------------------------------------------------------------------------+
| 🌌 Disaster Rover Ground Control Station Portal                                        |
+------------------------------------+--------------------------------------------------+
| [HUD COCKPIT FEED]                 | [MAPPING DASHBOARD (Leaflet JS)]                 |
|                                    |   (Plots live GPS nodes, records path tracks)    |
|   +----------------------------+   +--------------------------------------------------+
|   |                            |   | [ENVIRONMENT SENSORS PANEL]                      |
|   |     LIVE CAMERA STREAM     |   |   Temperature: 28.5 °C    Humidity: 62.4%        |
|   |                            |   |   MQ2 Smoke: SAFE         MQ9 Gas: SAFE          |
|   +----------------------------+   +--------------------------------------------------+
|   Manual Gimbal: [Pan] [Tilt]      | [ROBOTIC ARM CONTROLLER]                         |
|                                    |   Base Angle:     --[|||]--                      |
| [MOTOR CONTROL - TANK DRIVE]       |   Shoulder Joint: --[|||]--                      |
|            [FORWARD]               |   Elbow Joint:    --[|||]--                      |
|    [LEFT]    [STOP]    [RIGHT]     |   Gripper Claw:   --[|||]--                      |
|           [BACKWARD]               +--------------------------------------------------+
|   Power Regulator: ---[|||]---     | [SYSTEM SIRENS COMMANDS]                         |
|                                    |   [BUZZER GAS]   [BUZZER FIRE]   [MUTE SIREN]       |
+------------------------------------+--------------------------------------------------+
```
*   **Mapping Engine**: Renders a live **Leaflet.js Map** using OpenStreetMap tiles. It automatically centers on the rover's GPS coordinates and draws a telemetry path trace.
*   **Sensor Gauge Panels**: Formats DHT11 and MQ-gas readings on dynamic cards, lighting up in vibrant red if status transitions to `DANGER`.
*   **Manual Override controls**: Provides immediate sliders to control the 4-axis robotic arm, pan-tilt gimbal servos, and driving speed.

---

## 📱 Telegram Telemetry Vault

The universal Telegram bot provides a complete secondary command interface.

<div align="center">
  <img src="https://img.shields.io/badge/Telegram-Universal%20Bot-blue?style=for-the-badge&logo=telegram" alt="Telegram Badge"/>
</div>

### Bot Father Integration Commands
*   `/status` - Returns a Markdown summary of DHT11 metrics, gas concentrations, fire status, and active motor driving direction.
*   `/location` - Decodes coordinate details and generates an instant click-to-open **Google Maps tracking link**.
*   `/stop` - Immediately publishes an emergency halt (`S`) override on `rover/control/cmd` to cut motor power.
*   `/fire_siren` / `/mute` - Manual sound override commands.
*   `/toggle_live` - Activates/Deactivates automatic **10-second location ping updates**.

---

## 🚀 Getting Started

To flash the distributed firmware nodes and deploy the Ground Control Station:

### 📋 Prerequisites
*   **Arduino IDE** (or VS Code with PlatformIO).
*   Install the following libraries in Arduino IDE:
    *   `PubSubClient` (by Nick O'Leary)
    *   `UniversalTelegramBot` (by Brian Gallagnator)
    *   `ArduinoJson` (by Benoit Blanchon)
    *   `ESP32Servo` (by Kevin Harrington)
    *   `TinyGPS++` (by Mikal Hart)
    *   `DHT sensor library` (by Adafruit)

### ⚙️ Initial Configurations

1.  **WiFi Settings (All Nodes)**:
    Open `1esp.ino`, `2esp.ino`, and `3esp.ino` and configure your credentials:
    ```cpp
    const char* ssid     = "YOUR_SSID";
    const char* password = "YOUR_PASSWORD";
    ```

2.  **MQTT Broker Config (All Nodes)**:
    Point the brokers to your preferred server (Defaults to HiveMQ's public broker):
    ```cpp
    const char* mqtt_server = "broker.hivemq.com";
    const int mqtt_port     = 1883;
    ```

3.  **Telegram Bot Integration (ESP2 Specific)**:
    Use Telegram's [BotFather](https://t.me/botfather) to generate a bot, and get your Chat ID:
    ```cpp
    const char* botToken = "YOUR_BOT_TOKEN_FROM_BOTFATHER";
    const char* chatID   = "YOUR_PERSONAL_CHAT_ID";
    ```

4.  **Firebase Realtime Database (ESP2 Specific)**:
    Configure your Firebase Host endpoint to backup disaster logs:
    ```cpp
    const char* firebaseHost = "https://your-project-default-rtdb.firebaseio.com/";
    ```

### ⚡ Flashing the Nodes
1.  Connect **ESP32 Node 1** to your PC and upload `ESP32_codes/1esp.ino`.
2.  Connect **ESP32 Node 2** (with formatted FAT32 MicroSD card inserted) and upload `ESP32_codes/2esp.ino`.
3.  Connect **ESP32 Node 3** and upload `ESP32_codes/3esp.ino`.
4.  Open the Serial Monitors (Baud Rate: `115200`) to confirm that all nodes connect to WiFi and successfully establish the MQTT client loop.

---

## 🏆 ZYRO Hackathon Recognition

This project was conceived, designed, wired, and coded in a high-pressure environment during the **ZYRO Hackathon** at **Kalyani Government Engineering College**. 

Thanks to the modularity of the **distributed 3-ESP32 architecture** and the robust communication layers, the system won the **Best Beginners Team** award. The judges recognized:
*   The innovative division of labor across multiple microcontrollers.
*   The inclusion of a physical backup system (SD log) alongside live cloud storage (Firebase).
*   The redundant control options (Web GCS + Telegram Bot).

---

## 👥 Contributors

*   **Team Lead & Core Firmware Engineer**: Swarnadeep (Swarn)
*   **Electronics Specialist & Hardware Wireman**: ZYRO Team Partner
*   **Design & UI UX Engineer**: ZYRO Team Partner

*Feel free to star ⭐ this repository if you found this disaster rover system useful!*
