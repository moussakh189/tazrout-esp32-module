/*
 * TAZROUT IoT Irrigation System
 * Days 10-11: Multi-Sensor Integration
 * Developer: KHENFRI Moussa
 *
 * Sensors:
 *  - Moisture (potentiometer on GPIO 35)
 *  - Temperature & Humidity (DHT22 on GPIO 16)
 *
 * LEDs:
 *  - Red (GPIO 13)    - Dry soil
 *  - Yellow (GPIO 12) - Optimal
 *  - Green (GPIO 14)  - Wet soil
 */

#include <DHT.h>

// ── Pins ──────────────────────────────────────
const int MOISTURE_PIN = 35;
const int DHT_PIN = 4;
#define DHT_TYPE DHT22

const int LED_DRY     = 13;
const int LED_OPTIMAL = 12;
const int LED_WET     = 14;

DHT dht(DHT_PIN, DHT_TYPE);

// ── Moisture calibration (fixed for Wokwi) ────
const int ADC_DRY = 0;
const int ADC_WET = 4095;

// ── Thresholds ────────────────────────────────
const int BASE_THRESHOLD = 30;
int activeThreshold = 30;

// ── Filter ────────────────────────────────────
const int FILTER_SIZE = 5;
int filterBuf[FILTER_SIZE];
int filterIndex = 0;
int filterTotal = 0;

// ── State ─────────────────────────────────────
bool pumpActive = false;

// ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_DRY,     OUTPUT);
  pinMode(LED_OPTIMAL, OUTPUT);
  pinMode(LED_WET,     OUTPUT);

  digitalWrite(LED_DRY,     LOW);
  digitalWrite(LED_OPTIMAL, LOW);
  digitalWrite(LED_WET,     LOW);

  dht.begin();

  for (int i = 0; i < FILTER_SIZE; i++) filterBuf[i] = 0;

  Serial.println("TAZROUT Multi-Sensor Node Ready");
  Serial.println("Sensors: Moisture + Temperature + Humidity");
  Serial.println("-----------------------------------------------");

  delay(2000);  // DHT22 stabilization
}

// ──────────────────────────────────────────────
void loop() {

  int rawMoisture = analogRead(MOISTURE_PIN);
  int filteredMoisture = applyFilter(rawMoisture);
  int moisture = map(filteredMoisture, ADC_DRY, ADC_WET, 0, 100);
  moisture = constrain(moisture, 0, 100);

  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  bool dhtOK = !isnan(temp) && !isnan(humidity);

  adjustThreshold(temp, humidity, dhtOK);

  if (moisture < activeThreshold && !pumpActive) {
    pumpActive = true;
    Serial.println(">> PUMP ON - soil is dry!");
  }
  else if (moisture > 60 && pumpActive) {
    pumpActive = false;
    Serial.println(">> PUMP OFF - soil is wet.");
  }

  updateLEDs(moisture);
  printStatus(moisture, temp, humidity, dhtOK);

  delay(3000);  // DHT22 requires 2+ seconds between reads
}

// ──────────────────────────────────────────────
int applyFilter(int newVal) {
  filterTotal -= filterBuf[filterIndex];
  filterBuf[filterIndex] = newVal;
  filterTotal += newVal;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;
  return filterTotal / FILTER_SIZE;
}

// ──────────────────────────────────────────────
void adjustThreshold(float temp, float hum, bool valid) {
  if (!valid) {
    activeThreshold = BASE_THRESHOLD;
    return;
  }

  int adjustment = 0;

  if (temp > 30) adjustment += 5;
  else if (temp > 25) adjustment += 3;

  if (hum < 40) adjustment += 5;
  else if (hum < 60) adjustment += 2;

  activeThreshold = BASE_THRESHOLD + adjustment;
  activeThreshold = constrain(activeThreshold, 25, 50);
}

// ──────────────────────────────────────────────
void updateLEDs(int moisture) {
  digitalWrite(LED_DRY,     LOW);
  digitalWrite(LED_OPTIMAL, LOW);
  digitalWrite(LED_WET,     LOW);

  if (moisture < 30) {
    digitalWrite(LED_DRY, HIGH);
  }
  else if (moisture <= 70) {
    digitalWrite(LED_OPTIMAL, HIGH);
  }
  else {
    digitalWrite(LED_WET, HIGH);
  }
}

// ──────────────────────────────────────────────
void printStatus(int moisture, float temp, float hum, bool dhtOK) {
  Serial.print("Moisture: ");
  Serial.print(moisture);
  Serial.print("%  |  ");

  if (dhtOK) {
    Serial.print("Temp: ");
    Serial.print(temp, 1);
    Serial.print("C  |  Humidity: ");
    Serial.print(hum, 1);
    Serial.print("%  |  ");
  } else {
    Serial.print("Temp: ERROR  |  Humidity: ERROR  |  ");
  }

  Serial.print("Threshold: ");
  Serial.print(activeThreshold);
  Serial.print("%  |  Pump: ");
  Serial.println(pumpActive ? "ON" : "OFF");
}
