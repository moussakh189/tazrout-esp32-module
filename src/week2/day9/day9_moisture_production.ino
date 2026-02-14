/*
 * ========================================
 * TAZROUT IoT Irrigation System
 * Day 9: Moisture Sensor - Production Ready
 * ========================================
 * 
 * Developer: KHENFRI Moussa
 * Date: February 9, 2026
 * 
 * TAZROUT ENHANCEMENTS:
 * - Sensor health monitoring
 * - Irrigation decision hysteresis
 * - Data validation
 * - Visual bar graph
 * - Ready for MQTT integration (Week 3)
 */

// Hardware
const int MOISTURE_PIN = 35;
const int LED_DRY = 13;
const int LED_OPTIMAL = 12;
const int LED_WET = 14;

// Calibration
int moistureDry = 3200;
int moistureWet = 1400;

// Filtering
const int FILTER_SIZE = 10;
int readings[FILTER_SIZE];
int readIndex = 0;
int total = 0;

// Thresholds with hysteresis
const int PUMP_ON_THRESHOLD = 30;   // Turn ON below 30%
const int PUMP_OFF_THRESHOLD = 60;  // Turn OFF above 60%

// System state
bool pumpActive = false;
int lastMoisture = 50;
unsigned long lastChangeTime = 0;
unsigned long readingCount = 0;

// Sensor health
bool sensorHealthy = true;
unsigned long lastValidReading = 0;
const unsigned long SENSOR_TIMEOUT = 30000;  // 30 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LED_DRY, OUTPUT);
  pinMode(LED_OPTIMAL, OUTPUT);
  pinMode(LED_WET, OUTPUT);
  
  printHeader();
  
  for (int i = 0; i < FILTER_SIZE; i++) {
    readings[i] = 0;
  }
  
  calibrateMoistureSensor();
  
  Serial.println("\n>>> TAZROUT Production Moisture Monitoring <<<\n");
  lastValidReading = millis();
}

void loop() {
  // Read sensor
  int raw = analogRead(MOISTURE_PIN);
  int filtered = applyFilter(raw);
  int moisture = map(filtered, moistureDry, moistureWet, 0, 100);
  moisture = constrain(moisture, 0, 100);
  
  // Validate reading
  bool valid = validateReading(raw, moisture);
  
  if (valid) {
    // Make irrigation decision with hysteresis
    irrigationDecisionHysteresis(moisture);
    
    // Update LEDs
    updateStatusLEDs(moisture);
    
    // Display
    if (readingCount % 5 == 0) {
      displayComprehensive(raw, filtered, moisture);
    } else {
      displayCompact(moisture);
    }
    
    lastMoisture = moisture;
    lastValidReading = millis();
  } else {
    Serial.println("⚠ Invalid sensor reading - check connection!");
  }
  
  // Check sensor health
  checkSensorHealth();
  
  readingCount++;
  delay(2000);
}

int applyFilter(int newReading) {
  total = total - readings[readIndex];
  readings[readIndex] = newReading;
  total = total + newReading;
  readIndex = (readIndex + 1) % FILTER_SIZE;
  return total / FILTER_SIZE;
}

bool validateReading(int raw, int moisture) {
  // Check for sensor disconnection
  if (raw == 0 || raw == 4095) {
    sensorHealthy = false;
    return false;
  }
  
  // Check for unrealistic jump
  if (abs(moisture - lastMoisture) > 30 && readingCount > 10) {
    Serial.println("⚠ Suspicious reading - too much change!");
    return false;
  }
  
  sensorHealthy = true;
  return true;
}

void irrigationDecisionHysteresis(int moisture) {
  // Hysteresis prevents pump oscillation
  if (moisture < PUMP_ON_THRESHOLD && !pumpActive) {
    pumpActive = true;
    lastChangeTime = millis();
    Serial.println("\n🚰 PUMP ACTIVATED - Soil is dry!");
  } else if (moisture > PUMP_OFF_THRESHOLD && pumpActive) {
    pumpActive = false;
    lastChangeTime = millis();
    Serial.println("\n✓ PUMP DEACTIVATED - Soil moisture adequate!");
  }
}

void updateStatusLEDs(int moisture) {
  digitalWrite(LED_DRY, LOW);
  digitalWrite(LED_OPTIMAL, LOW);
  digitalWrite(LED_WET, LOW);
  
  if (moisture < 30) {
    digitalWrite(LED_DRY, HIGH);
  } else if (moisture < 70) {
    digitalWrite(LED_OPTIMAL, HIGH);
  } else {
    digitalWrite(LED_WET, HIGH);
  }
}

void checkSensorHealth() {
  if (millis() - lastValidReading > SENSOR_TIMEOUT) {
    Serial.println("\n🔴 SENSOR TIMEOUT - No valid readings for 30s!");
    sensorHealthy = false;
  }
}

void displayCompact(int moisture) {
  Serial.print("Moisture: ");
  Serial.print(moisture);
  Serial.print("% | Pump: ");
  Serial.print(pumpActive ? "ON ✓" : "OFF");
  Serial.print(" | Sensor: ");
  Serial.println(sensorHealthy ? "OK ✓" : "ERROR ✗");
}

void displayComprehensive(int raw, int filtered, int moisture) {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║     TAZROUT MOISTURE REPORT            ║");
  Serial.println("╠════════════════════════════════════════╣");
  
  Serial.print("║ Moisture Level:  ");
  Serial.print(moisture);
  Serial.print("%");
  if (moisture < 10) Serial.print("  ");
  else if (moisture < 100) Serial.print(" ");
  
  // Visual bar
  Serial.print(" ");
  printBarGraph(moisture, 15);
  Serial.println(" ║");
  
  Serial.print("║ Status:          ");
  if (moisture < 30) Serial.print("DRY (Need Water)  ");
  else if (moisture < 70) Serial.print("OPTIMAL           ");
  else Serial.print("WET (Too Much)    ");
  Serial.println("║");
  
  Serial.print("║ Pump Status:     ");
  Serial.print(pumpActive ? "ACTIVE ✓          " : "STANDBY           ");
  Serial.println("║");
  
  Serial.print("║ Sensor Health:   ");
  Serial.print(sensorHealthy ? "HEALTHY ✓         " : "ERROR ✗           ");
  Serial.println("║");
  
  Serial.print("║ Raw ADC:         ");
  Serial.print(raw);
  if (raw < 1000) Serial.print(" ");
  if (raw < 100) Serial.print(" ");
  if (raw < 10) Serial.print(" ");
  Serial.print(" (Filtered: ");
  Serial.print(filtered);
  Serial.println(") ║");
  
  Serial.print("║ Reading Count:   ");
  Serial.print(readingCount);
  for (int i = String(readingCount).length(); i < 20; i++) Serial.print(" ");
  Serial.println("║");
  
  Serial.println("╚════════════════════════════════════════╝\n");
}

void printBarGraph(int value, int width) {
  Serial.print("[");
  int filled = (value * width) / 100;
  for (int i = 0; i < width; i++) {
    if (i < filled) Serial.print("█");
    else Serial.print("░");
  }
  Serial.print("]");
}

void calibrateMoistureSensor() {
  Serial.println("⚙ Calibrating moisture sensor...");
  
  unsigned long startTime = millis();
  int tempMin = 4095, tempMax = 0;
  
  while (millis() - startTime < 3000) {
    int reading = analogRead(MOISTURE_PIN);
    if (reading < tempMin) tempMin = reading;
    if (reading > tempMax) tempMax = reading;
    delay(10);
  }
  
  moistureDry = tempMax + 10;
  moistureWet = tempMin - 10;
  moistureDry = constrain(moistureDry, 0, 4095);
  moistureWet = constrain(moistureWet, 0, 4095);
  
  Serial.print("✓ Calibration complete: Dry=");
  Serial.print(moistureDry);
  Serial.print(", Wet=");
  Serial.println(moistureWet);
}

void printHeader() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║      TAZROUT IRRIGATION SYSTEM         ║");
  Serial.println("║   Production Moisture Sensor Module    ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║  Features:                             ║");
  Serial.println("║  • Hysteresis control                  ║");
  Serial.println("║  • Sensor validation                   ║");
  Serial.println("║  • Health monitoring                   ║");
  Serial.println("║  • Visual feedback                     ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.println("Irrigation Logic:");
  Serial.println("  • Moisture < 30% → Pump ON");
  Serial.println("  • Moisture > 60% → Pump OFF");
  Serial.println("  • 30-60% → Maintain current state\n");
}
