#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// === Hardware Pin Definitions ===
#define FAN_PIN 15
#define PUMP_PIN 2
#define FLOAT_1_SWITCH_PIN 7
#define FLOAT_2_SWITCH_PIN 9
#define SENSOR_PIN 8

// === Temperature Thresholds ===
const float TEMP_ON_THRESHOLD = 26.0;
const float TEMP_OFF_THRESHOLD = 24.5;

// === Pump Safety Timing ===
const unsigned long MAX_PUMP_RUNTIME_MS = 5000; // 30 seconds

// === OLED Display Settings ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool displayAvailable = false;

// === State Variables ===
bool fanOn = false;
bool pumpOn = false;
bool pumpLockout = false;
unsigned long pumpStartTime = 0;
float currentTempC = 0.0;

// === Dallas Temperature Sensor ===
OneWire oneWire(SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

// === Function Declarations ===
void checkTemperature();
void handlePump();
void updateDisplay();

void setup() {
  // === Initialize GPIOs ===
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FLOAT_1_SWITCH_PIN, INPUT_PULLUP);
  pinMode(FLOAT_2_SWITCH_PIN, INPUT_PULLUP);

  digitalWrite(FAN_PIN, LOW);   // Ensure fan is off
  digitalWrite(PUMP_PIN, LOW);  // Ensure pump is off

  // === Start Serial Communication ===
  Serial.begin(9600);
  // while (!Serial);  // Optional: Wait for serial (useful during debug)

  // === Initialize Temperature Sensor ===
  tempSensor.begin();

  // === Set I2C Pins and Begin I2C Bus ===
  Wire.begin(); // SDA, SCL pins for Raspberry Pi Pico

  // === Initialize OLED Display ===
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    displayAvailable = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Display Initialized");
    display.display();
    delay(1000);
  } else {
    Serial.println("OLED display not found, continuing without it.");
  }

  Serial.println("System Initialized. Monitoring started.");
}

void loop() {
  checkTemperature();
  handlePump();
  updateDisplay();
  delay(1000);  // 1 second cycle
}

// === Temperature-Based Fan Control ===
void checkTemperature() {
  tempSensor.requestTemperatures();
  currentTempC = tempSensor.getTempCByIndex(0);
  Serial.print("Temperature: ");
  Serial.print(currentTempC);
  Serial.println(" °C");

  if (!fanOn && currentTempC >= TEMP_ON_THRESHOLD) {
    digitalWrite(FAN_PIN, HIGH);
    fanOn = true;
    Serial.println("Fan ON");
  } else if (fanOn && currentTempC <= TEMP_OFF_THRESHOLD) {
    digitalWrite(FAN_PIN, LOW);
    fanOn = false;
    Serial.println("Fan OFF");
  }
}

// === Pump Control with Safety Lockout ===
void handlePump() {
  if (pumpLockout) {
    Serial.println("Pump locked out. Awaiting manual reset.");
    return;
  }

  bool floatSwitchTriggered = (digitalRead(FLOAT_1_SWITCH_PIN) == LOW && digitalRead(FLOAT_2_SWITCH_PIN) == LOW);

  if (floatSwitchTriggered) {
    if (!pumpOn) {
      digitalWrite(PUMP_PIN, HIGH);
      pumpOn = true;
      pumpStartTime = millis();
      Serial.println("Pump ON");
    } else if (millis() - pumpStartTime > MAX_PUMP_RUNTIME_MS) {
      digitalWrite(PUMP_PIN, LOW);
      pumpOn = false;
      pumpLockout = true;  // Enter lockout
      Serial.println("Pump OFF - Safety Lockout Triggered");
    }
  } else {
    if (pumpOn) {
      digitalWrite(PUMP_PIN, LOW);
      pumpOn = false;
      Serial.println("Pump OFF");
    }
  }
}

// === OLED Display Update (Optional) ===
void updateDisplay() {
  if (!displayAvailable) return;  // Skip if display is not found

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(currentTempC);
  display.println(" C");

  display.print("Fan: ");
  display.println(fanOn ? "ON" : "OFF");

  display.print("Pump: ");
  display.println(pumpOn ? "ON" : "OFF");

  if (pumpLockout) {
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Optional inverse text for alert
    display.println("*** LOCKOUT ***");
    display.println("CHECK TANK");
  }

  display.display();
}
