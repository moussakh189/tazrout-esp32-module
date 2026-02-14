/*
 * ========================================
 * TAZROUT IoT Irrigation System
 * Days 12-14: Complete Sensor Node
 * ========================================
 * 
 * Developer: KHENFRI Moussa
 * Date: February 12-14, 2026
 * 
 * FINAL TAZROUT SENSOR NODE - WEEK 2 DELIVERABLE
 * 
 * Features:
 * - Multi-sensor (Moisture, Temperature, Humidity)
 * - Data logging (10-reading circular buffer)
 * - Statistics (min/max/avg)
 * - JSON output (ready for MQTT Week 3)
 * - Weather-adaptive irrigation
 * - Production-ready code
 */

#include <DHT.h>
#include <ArduinoJson.h>

// Hardware
const int MOISTURE_PIN = 35;
const int DHT_PIN = 16;
#define DHT_TYPE DHT22
const int LED_DRY = 13;
const int LED_OPTIMAL = 12;
const int LED_WET = 14;

DHT dht(DHT_PIN, DHT_TYPE);

// Data structure
struct SensorReading {
  int moisture;
  float temperature;
  float humidity;
  unsigned long timestamp;
  bool valid;
};

// Circular buffer
const int BUFFER_SIZE = 10;
SensorReading dataBuffer[BUFFER_SIZE];
int bufferIndex = 0;
int bufferCount = 0;

// Current state
SensorReading current;
bool pumpActive = false;
int pumpOnThreshold = 30;

// Calibration
int moistureDry = 3200;
int moistureWet = 1400;

// Filter
const int FILTER_SIZE = 10;
int moistureReadings[FILTER_SIZE];
int readIndex = 0;
int total = 0;

unsigned long readingNumber = 0;
const char* NODE_ID = "TAZROUT_ESP32_001";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LED_DRY, OUTPUT);
  pinMode(LED_OPTIMAL, OUTPUT);
  pinMode(LED_WET, OUTPUT);
  
  dht.begin();
  
  printHeader();
  calibrateMoisture();
  
  for (int i = 0; i < FILTER_SIZE; i++) {
    moistureReadings[i] = 0;
  }
  
  Serial.println("\n>>> TAZROUT Complete Sensor Node Active <<<");
  Serial.println(">>> Ready for MQTT Integration (Week 3) <<<\n");
  delay(2000);
}

void loop() {
  // Read all sensors
  readSensors();
  
  // Add to circular buffer
  addToBuffer(current);
  
  // Weather-adaptive threshold
  adjustThreshold();
  
  // Irrigation decision
  irrigationLogic();
  
  // Update LEDs
  updateLEDs();
  
  // Display every 3rd reading
  if (readingNumber % 3 == 0) {
    displaySensorData();
  }
  
  // Statistics every 10 readings
  if (readingNumber % 10 == 0 && bufferCount >= 5) {
    displayStatistics();
  }
  
  // JSON output every 5 readings
  if (readingNumber % 5 == 0) {
    outputJSON();
  }
  
  readingNumber++;
  delay(3000);
}

void readSensors() {
  // Moisture
  int raw = analogRead(MOISTURE_PIN);
  int filtered = applyFilter(raw);
  current.moisture = map(filtered, moistureDry, moistureWet, 0, 100);
  current.moisture = constrain(current.moisture, 0, 100);
  
  // DHT22
  current.temperature = dht.readTemperature();
  current.humidity = dht.readHumidity();
  
  // Validate
  current.valid = (raw > 0 && raw < 4095) && 
                  !isnan(current.temperature) && 
                  !isnan(current.humidity);
  
  if (!current.valid && !isnan(current.temperature)) {
    current.valid = true;  // Partial validity
  }
  
  current.timestamp = millis();
}

int applyFilter(int newReading) {
  total = total - moistureReadings[readIndex];
  moistureReadings[readIndex] = newReading;
  total = total + newReading;
  readIndex = (readIndex + 1) % FILTER_SIZE;
  return total / FILTER_SIZE;
}

void addToBuffer(SensorReading reading) {
  dataBuffer[bufferIndex] = reading;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
  if (bufferCount < BUFFER_SIZE) bufferCount++;
}

void adjustThreshold() {
  if (!current.valid) return;
  
  int adjustment = 0;
  if (current.temperature > 30) adjustment += 5;
  else if (current.temperature > 25) adjustment += 3;
  
  if (current.humidity < 40) adjustment += 5;
  else if (current.humidity < 60) adjustment += 2;
  
  pumpOnThreshold = 30 + adjustment;
  pumpOnThreshold = constrain(pumpOnThreshold, 25, 50);
}

void irrigationLogic() {
  if (current.moisture < pumpOnThreshold && !pumpActive) {
    pumpActive = true;
    Serial.println("\n🚰 IRRIGATION ACTIVATED");
  } else if (current.moisture > 60 && pumpActive) {
    pumpActive = false;
    Serial.println("\n✓ IRRIGATION STOPPED");
  }
}

void updateLEDs() {
  digitalWrite(LED_DRY, current.moisture < 30);
  digitalWrite(LED_OPTIMAL, current.moisture >= 30 && current.moisture < 70);
  digitalWrite(LED_WET, current.moisture >= 70);
}

void displaySensorData() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║      TAZROUT SENSOR READINGS           ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.printf("║ Moisture:     %3d%% %-18s║\n", current.moisture, getMoistureStatus());
  if (current.valid) {
    Serial.printf("║ Temperature:  %.1f°C                     ║\n", current.temperature);
    Serial.printf("║ Humidity:     %.1f%%                      ║\n", current.humidity);
  }
  Serial.printf("║ Pump:         %-22s║\n", pumpActive ? "ACTIVE ✓" : "STANDBY");
  Serial.printf("║ Threshold:    %d%% (adaptive)             ║\n", pumpOnThreshold);
  Serial.printf("║ Reading:      #%-20lu║\n", readingNumber);
  Serial.println("╚════════════════════════════════════════╝\n");
}

const char* getMoistureStatus() {
  if (current.moisture < 30) return "[DRY]";
  if (current.moisture < 70) return "[OPTIMAL]";
  return "[WET]";
}

void displayStatistics() {
  if (bufferCount < 5) return;
  
  int moistureSum = 0, moistureMin = 100, moistureMax = 0;
  float tempSum = 0, tempMin = 999, tempMax = -999;
  float humSum = 0;
  
  for (int i = 0; i < bufferCount; i++) {
    moistureSum += dataBuffer[i].moisture;
    if (dataBuffer[i].moisture < moistureMin) moistureMin = dataBuffer[i].moisture;
    if (dataBuffer[i].moisture > moistureMax) moistureMax = dataBuffer[i].moisture;
    
    if (dataBuffer[i].valid) {
      tempSum += dataBuffer[i].temperature;
      humSum += dataBuffer[i].humidity;
      if (dataBuffer[i].temperature < tempMin) tempMin = dataBuffer[i].temperature;
      if (dataBuffer[i].temperature > tempMax) tempMax = dataBuffer[i].temperature;
    }
  }
  
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║     STATISTICS (Last 10 Readings)      ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.printf("║ Moisture:  Min=%d%%  Avg=%d%%  Max=%d%%  ║\n", 
                moistureMin, moistureSum/bufferCount, moistureMax);
  Serial.printf("║ Temp:      Min=%.1f°C Avg=%.1f°C Max=%.1f°C║\n",
                tempMin, tempSum/bufferCount, tempMax);
  Serial.printf("║ Humidity:  Avg=%.1f%%                   ║\n", humSum/bufferCount);
  Serial.println("╚════════════════════════════════════════╝\n");
}

void outputJSON() {
  StaticJsonDocument<512> doc;
  
  doc["node_id"] = NODE_ID;
  doc["timestamp"] = current.timestamp;
  doc["reading_number"] = readingNumber;
  
  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["moisture"]["value"] = current.moisture;
  sensors["moisture"]["status"] = getMoistureStatus();
  sensors["moisture"]["valid"] = true;
  
  if (current.valid) {
    sensors["temperature"]["value"] = round(current.temperature * 10) / 10.0;
    sensors["temperature"]["unit"] = "celsius";
    sensors["temperature"]["valid"] = true;
    
    sensors["humidity"]["value"] = round(current.humidity * 10) / 10.0;
    sensors["humidity"]["unit"] = "percent";
    sensors["humidity"]["valid"] = true;
  }
  
  JsonObject irrigation = doc.createNestedObject("irrigation");
  irrigation["pump_active"] = pumpActive;
  irrigation["threshold"] = pumpOnThreshold;
  irrigation["mode"] = "auto";
  
  JsonObject system = doc.createNestedObject("system");
  system["uptime_ms"] = millis();
  system["buffer_count"] = bufferCount;
  
  Serial.println("\n========== JSON OUTPUT (MQTT Ready) ==========");
  serializeJsonPretty(doc, Serial);
  Serial.println("\n==============================================\n");
}

void calibrateMoisture() {
  Serial.println("⚙ Calibrating...");
  unsigned long start = millis();
  int tempMin = 4095, tempMax = 0;
  
  while (millis() - start < 3000) {
    int reading = analogRead(MOISTURE_PIN);
    if (reading < tempMin) tempMin = reading;
    if (reading > tempMax) tempMax = reading;
    delay(10);
  }
  
  moistureDry = tempMax + 10;
  moistureWet = tempMin - 10;
  Serial.println("✓ Calibration complete");
}

void printHeader() {
  Serial.println("\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║                                        ║");
  Serial.println("║          TAZROUT SENSOR NODE           ║");
  Serial.println("║      Week 2 Final Deliverable          ║");
  Serial.println("║                                        ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║  Complete Agricultural Monitoring      ║");
  Serial.println("║  • Soil Moisture (Smart)               ║");
  Serial.println("║  • Temperature & Humidity              ║");
  Serial.println("║  • Data Logging (10 readings)          ║");
  Serial.println("║  • JSON Output (MQTT Ready)            ║");
  Serial.println("║  • Weather-Adaptive Control            ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║  Developer: KHENFRI Moussa             ║");
  Serial.println("║  Date: February 12-14, 2026            ║");
  Serial.println("║  Status: Production Ready              ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}
