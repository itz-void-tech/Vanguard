#include <ESP32Servo.h>

// ======================================================
// FLAME SENSOR PINS
// ======================================================

#define FLAME1 34
#define FLAME2 35
#define FLAME3 32

// ======================================================
// RELAY
// ======================================================

#define RELAY_PIN 27

// ======================================================
// SERVO
// ======================================================

#define SERVO_PIN 13

Servo sprayServo;

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  // Flame Sensors
  pinMode(FLAME1, INPUT);
  pinMode(FLAME2, INPUT);
  pinMode(FLAME3, INPUT);

  // Relay
  pinMode(RELAY_PIN, OUTPUT);

  // RELAY OFF INITIALLY
  digitalWrite(RELAY_PIN, LOW);

  // Servo Setup
  sprayServo.setPeriodHertz(50);
  sprayServo.attach(SERVO_PIN, 500, 2400);

  // Start at center
  sprayServo.write(90);

  Serial.println("================================");
  Serial.println(" FIRE FIGHTING SYSTEM READY ");
  Serial.println("================================");
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  bool fireDetected = isFireDetected();

  // ==================================================
  // FIRE DETECTED
  // ==================================================

  if (fireDetected) {

    Serial.println("FIRE DETECTED!");

    // RELAY ON
    digitalWrite(RELAY_PIN, HIGH);

    // Sweep LEFT -> RIGHT
    for (int pos = 0; pos <= 180; pos += 2) {

      sprayServo.write(pos);

      delay(15);

      // Stop immediately if fire gone
      if (!isFireDetected()) {
        stopSystem();
        return;
      }
    }

    // Sweep RIGHT -> LEFT
    for (int pos = 180; pos >= 0; pos -= 2) {

      sprayServo.write(pos);

      delay(15);

      if (!isFireDetected()) {
        stopSystem();
        return;
      }
    }
  }

  // ==================================================
  // NO FIRE
  // ==================================================

  else {

    stopSystem();
  }
}

// ======================================================
// FIRE DETECTION FUNCTION
// ======================================================

bool isFireDetected() {

  bool fire1 = digitalRead(FLAME1) == LOW;
  bool fire2 = digitalRead(FLAME2) == LOW;
  bool fire3 = digitalRead(FLAME3) == LOW;

  return (fire1 || fire2 || fire3);
}

// ======================================================
// STOP SYSTEM
// ======================================================

void stopSystem() {

  // RELAY OFF
  digitalWrite(RELAY_PIN, LOW);

  // Servo Center
  sprayServo.write(90);

  Serial.println("NO FIRE");

  delay(100);
}