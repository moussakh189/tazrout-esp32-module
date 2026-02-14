/*
 * ========================================
 * TAZROUT IoT Irrigation System
 * Days 10-11: Multi-Sensor Integration
 * ========================================
 * 
 * Developer: KHENFRI Moussa
 * Date: February 10-11, 2026
 * 
 * TAZROUT COMPLETE SENSOR NODE:
 * - Moisture sensor (irrigation decisions)
 * - DHT22 temperature (evaporation rate)
 * - DHT22 humidity (water loss prediction)
 * 
 * Agricultural Logic:
 * - High temp + low humidity = faster water loss
 * - Adjust irrigation based on weather conditions
 */

#include <DHT.h>

// ============================================
// HARDWARE CONFIGURATION
// ============================================
const int MOISTURE_PIN = 35;
const int DHT_PIN = 16;
#define DHT_TYPE DHT22

const int LED_DRY = 13;
const int LED_OPTIMAL = 12;
const int LED_WET = 14;

DHT dht(DHT_PIN, DHT_TYPE);

// ============================================
// SENSOR DATA STRUCTURE
// ============================================
struct SensorData {
  int moisture;
  float temperature;
  float humidity;
  unsigned long timestamp;
  bool moistureValid;
  bool dhtValid;
  bool pumpActive;
};

SensorData currentData;

// Calibration
int moistureDry = 3200;
int moistureWet = 1400;

// Filtering
const int FILTER_SIZE = 10;
int moistureReadings[FILTER_SIZE];
int readIndex = 0;
int total = 0;

// Thresholds
int pumpOnThreshold = 30;   // Dynamic threshold
const int BASE_THRESHOLD = 30;

// State
bool pumpActive = false;
unsigned long readingCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize hardware
  pinMode(LED_DRY, OUTPUT);
  pinMode(LED_OPTIMAL, OUTPUT);
  pinMode(LED_WET, OUTPUT);
  
  dht.begin();
  
  printHeader();
  
  // Initialize filter
  for (int i = 0; i < FILTER_SIZE; i++) {
    moistureReadings[i] = 0;
  }
  
  calibrateMoisture();
  
  Serial.println("\n>>> TAZROUT Multi-Sensor Node Active <<<\n");
  delay(2000);  // DHT needs time to stabilize
}

void loop() {
  // Read all sensors
  readAllSensors();
  
  // Adjust threshold based on weather
  adjustThresholdForWeather();
  
  // Make irrigation decision
  irrigationDecision();
  
  // Update visual feedback
  updateLEDs();
  
  // Display
  if (readingCount % 3 == 0) {
    displayDetailed();
  } else {
    displayCompact();
  }
  
  readingCount++;
  delay(3000);  // DHT22 needs 2-3 seconds between reads
}

// ============================================
// READ ALL SENSORS
// ============================================
void readAllSensors() {
  // Read moisture
  int raw = analogRead(MOISTURE_PIN);
  int filtered = applyFilter(raw);
  currentData.moisture = map(filtered, moistureDry, moistureWet, 0, 100);
  currentData.moisture = constrain(currentData.moisture, 0, 100);
  currentData.moistureValid = (raw > 0 && raw < 4095);
  
  // Read DHT22
  currentData.temperature = dht.readTemperature();
  currentData.humidity = dht.readHumidity();
  
  // Validate DHT
  currentData.dhtValid = !isnan(currentData.temperature) && 
                         !isnan(currentData.humidity);
  
  // Retry DHT if failed
  if (!currentData.dhtValid) {
    delay(500);
    currentData.temperature = dht.readTemperature();
    currentData.humidity = dht.readHumidity();
    currentData.dhtValid = !isnan(currentData.temperature) && 
                           !isnan(currentData.humidity);
  }
  
  currentData.timestamp = millis();
  currentData.pumpActive = pumpActive;
}

// ============================================
// WEATHER-ADAPTIVE THRESHOLD
// ============================================
void adjustThresholdForWeather() {
  if (!currentData.dhtValid) {
    pumpOnThreshold = BASE_THRESHOLD;
    return;
  }
  
  // High temperature or low humidity = faster evaporation
  // Increase threshold to irrigate earlier
  
  int adjustment = 0;
  
  // Temperature adjustment
  if (currentData.temperature > 30) {
    adjustment += 5;  // Very hot → irrigate earlier
  } else if (currentData.temperature > 25) {
    adjustment += 3;  // Warm → slight adjustment
  }
  
  // Humidity adjustment
  if (currentData.humidity < 40) {
    adjustment += 5;  // Very dry air → irrigate earlier
  } else if (currentData.humidity < 60) {
    adjustment += 2;  // Low humidity → slight adjustment
  }
  
  pumpOnThreshold = BASE_THRESHOLD + adjustment;
  pumpOnThreshold = constrain(pumpOnThreshold, 25, 50);
}

// ============================================
// IRRIGATION DECISION
// ============================================
void irrigationDecision() {
  if (!currentData.moistureValid) return;
  
  // Activate pump if moisture below threshold
  if (currentData.moisture < pumpOnThreshold && !pumpActive) {
    pumpActive = true;
    Serial.println("\n🚰 PUMP ACTIVATED");
    Serial.print("   Reason: Moisture ");
    Serial.print(currentData.moisture);
    Serial.print("% < ");
    Serial.print(pumpOnThreshold);
    Serial.println("%");
    
    if (currentData.dhtValid) {
      Serial.print("   Weather: ");
      Serial.print(currentData.temperature, 1);
      Serial.print("°C, ");
      Serial.print(currentData.humidity, 1);
      Serial.println("% RH");
    }
  }
  // Deactivate if moisture above 60%
  else if (currentData.moisture > 60 && pumpActive) {
    pumpActive = false;
    Serial.println("\n✓ PUMP DEACTIVATED - Adequate moisture");
  }
}

// ============================================
// FILTER & LEDS
// ============================================
int applyFilter(int newReading) {
  total = total - moistureReadings[readIndex];
  moistureReadings[readIndex] = newReading;
  total = total + newReading;
  readIndex = (readIndex + 1) % FILTER_SIZE;
  return total / FILTER_SIZE;
}

void updateLEDs() {
  digitalWrite(LED_DRY, LOW);
  digitalWrite(LED_OPTIMAL, LOW);
  digitalWrite(LED_WET, LOW);
  
  if (currentData.moisture < 30) digitalWrite(LED_DRY, HIGH);
  else if (currentData.moisture < 70) digitalWrite(LED_OPTIMAL, HIGH);
  else digitalWrite(LED_WET, HIGH);
}

// ============================================
// DISPLAY FUNCTIONS
// ============================================
void displayCompact() {
  Serial.print("M:");
  Serial.print(currentData.moisture);
  Serial.print("% | ");
  
  if (currentData.dhtValid) {
    Serial.print("T:");
    Serial.print(currentData.temperature, 1);
    Serial.print("°C | H:");
    Serial.print(currentData.humidity, 1);
    Serial.print("% | ");
  }
  
  Serial.print("Pump:");
  Serial.print(pumpActive ? "ON" : "OFF");
  Serial.print(" | Threshold:");
  Serial.print(pumpOnThreshold);
  Serial.println("%");
}

void displayDetailed() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   TAZROUT AGRICULTURAL SENSOR DATA     ║");
  Serial.println("╠════════════════════════════════════════╣");
  
  // Moisture
  Serial.print("║ Soil Moisture:   ");
  printValue(currentData.moisture, "%", currentData.moistureValid);
  
  // Temperature
  Serial.print("║ Temperature:     ");
  if (currentData.dhtValid) {
    Serial.print(currentData.temperature, 1);
    Serial.print("°C");
    printSpaces(13 - String((int)currentData.temperature).length());
    Serial.println("✓ ║");
  } else {
    Serial.println("ERROR        ✗ ║");
  }
  
  // Humidity
  Serial.print("║ Air Humidity:    ");
  if (currentData.dhtValid) {
    Serial.print(currentData.humidity, 1);
    Serial.print("%");
    printSpaces(14 - String((int)currentData.humidity).length());
    Serial.println("✓ ║");
  } else {
    Serial.println("ERROR        ✗ ║");
  }
  
  Serial.println("╠════════════════════════════════════════╣");
  
  // Weather-adjusted threshold
  Serial.print("║ Active Threshold: ");
  Serial.print(pumpOnThreshold);
  Serial.print("%");
  if (pumpOnThreshold != BASE_THRESHOLD) {
    Serial.print(" (adjusted)");
  }
  printSpaces(17 - String(pumpOnThreshold).length() - (pumpOnThreshold != BASE_THRESHOLD ? 11 : 0));
  Serial.println("║");
  
  // Pump status
  Serial.print("║ Pump Status:     ");
  if (pumpActive) {
    Serial.println("ACTIVE ✓            ║");
  } else {
    Serial.println("STANDBY             ║");
  }
  
  Serial.println("╚════════════════════════════════════════╝\n");
}

void printValue(int value, const char* unit, bool valid) {
  if (valid) {
    Serial.print(value);
    Serial.print(unit);
    printSpaces(16 - String(value).length() - String(unit).length());
    Serial.println("✓ ║");
  } else {
    Serial.println("ERROR        ✗ ║");
  }
}

void printSpaces(int count) {
  for (int i = 0; i < count; i++) Serial.print(" ");
}

// ============================================
// CALIBRATION & HEADER
// ============================================
void calibrateMoisture() {
  Serial.println("⚙ Calibrating moisture sensor...");
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
  moistureDry = constrain(moistureDry, 0, 4095);
  moistureWet = constrain(moistureWet, 0, 4095);
  
  Serial.println("✓ Calibration complete");
}

void printHeader() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║     TAZROUT MULTI-SENSOR NODE          ║");
  Serial.println("║   Complete Agricultural Monitoring     ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║  Sensors:                              ║");
  Serial.println("║  • Soil Moisture (Capacitive)          ║");
  Serial.println("║  • Air Temperature (DHT22)             ║");
  Serial.println("║  • Air Humidity (DHT22)                ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║  Smart Irrigation:                     ║");
  Serial.println("║  • Weather-adaptive thresholds         ║");
  Serial.println("║  • Temperature compensation            ║");
  Serial.println("║  • Humidity-based adjustment           ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}
