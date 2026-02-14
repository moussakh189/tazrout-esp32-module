/*
 * ========================================
 * TAZROUT IoT Irrigation System
 * Day 8: Moisture Sensor Integration
 * ========================================
 * 
 * Developer: KHENFRI Moussa
 * Date: February 8, 2026
 * Module: ESP32 Sensor Node
 * 
 * TAZROUT CONNECTION:
 * This is THE CORE SENSOR for irrigation decisions.
 * Moisture readings directly control pump activation.
 * 
 * Features:
 * - Capacitive moisture sensor simulation
 * - Automatic calibration (dry/wet)
 * - Moving average filter (10 readings)
 * - Irrigation decision logic
 * - Status LED indicators
 */

// ============================================
// HARDWARE CONFIGURATION
// ============================================
const int MOISTURE_PIN = 35;        // GPIO 35 - ADC1_CH7
const int LED_DRY = 13;             // Red LED - Needs water
const int LED_OPTIMAL = 12;         // Yellow LED - Good
const int LED_WET = 14;             // Green LED - Too much

// ============================================
// CALIBRATION VALUES
// ============================================
int moistureDry = 3200;   // ADC value in dry air (will calibrate)
int moistureWet = 1400;   // ADC value in water (will calibrate)

// ============================================
// FILTERING CONFIGURATION
// ============================================
const int FILTER_SIZE = 10;         // 10 readings average
int readings[FILTER_SIZE];
int readIndex = 0;
int total = 0;

// ============================================
// TAZROUT IRRIGATION THRESHOLDS
// ============================================
const int THRESHOLD_DRY = 30;       // Below 30% = Activate pump
const int THRESHOLD_OPTIMAL_LOW = 30;
const int THRESHOLD_OPTIMAL_HIGH = 70;
const int THRESHOLD_WET = 70;       // Above 70% = Stop pump

// ============================================
// SYSTEM STATUS
// ============================================
unsigned long readingCount = 0;
bool pumpShouldActivate = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize LEDs
  pinMode(LED_DRY, OUTPUT);
  pinMode(LED_OPTIMAL, OUTPUT);
  pinMode(LED_WET, OUTPUT);
  
  // Turn off all LEDs
  digitalWrite(LED_DRY, LOW);
  digitalWrite(LED_OPTIMAL, LOW);
  digitalWrite(LED_WET, LOW);
  
  // Print header
  printHeader();
  
  // Initialize filter
  for (int i = 0; i < FILTER_SIZE; i++) {
    readings[i] = 0;
  }
  
  // Run calibration
  calibrateMoistureSensor();
  
  Serial.println("\n>>> TAZROUT Moisture Monitoring Active <<<\n");
}

void loop() {
  // Read and filter moisture
  int rawMoisture = analogRead(MOISTURE_PIN);
  int filtered = applyFilter(rawMoisture);
  
  // Convert to percentage (inverted: low ADC = high moisture)
  int moisturePercent = map(filtered, moistureDry, moistureWet, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);
  
  // Make irrigation decision
  irrigationDecision(moisturePercent);
  
  // Update LEDs
  updateStatusLEDs(moisturePercent);
  
  // Display
  displayMoistureData(rawMoisture, filtered, moisturePercent);
  
  readingCount++;
  delay(2000);  // Read every 2 seconds
}

// ============================================
// CALIBRATION FUNCTION
// ============================================
void calibrateMoistureSensor() {
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   MOISTURE SENSOR CALIBRATION MODE    ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ Simulating dry → wet transition       ║");
  Serial.println("║ Rotate potentiometer through range... ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  unsigned long startTime = millis();
  int tempMin = 4095;
  int tempMax = 0;
  
  // Sample for 3 seconds
  while (millis() - startTime < 3000) {
    int reading = analogRead(MOISTURE_PIN);
    
    if (reading < tempMin) tempMin = reading;
    if (reading > tempMax) tempMax = reading;
    
    delay(10);
  }
  
  // Set calibration (inverted for capacitive sensor)
  moistureDry = tempMax + 10;  // Dry = higher ADC
  moistureWet = tempMin - 10;  // Wet = lower ADC
  
  moistureDry = constrain(moistureDry, 0, 4095);
  moistureWet = constrain(moistureWet, 0, 4095);
  
  Serial.println("✓ Calibration Complete!");
  Serial.print("  Dry Air (0%):  ADC = ");
  Serial.println(moistureDry);
  Serial.print("  Water (100%):  ADC = ");
  Serial.println(moistureWet);
  Serial.print("  Range:         ");
  Serial.println(moistureDry - moistureWet);
  
  if (moistureDry - moistureWet < 100) {
    Serial.println("\n⚠ WARNING: Small calibration range detected!");
  }
}

// ============================================
// MOVING AVERAGE FILTER
// ============================================
int applyFilter(int newReading) {
  total = total - readings[readIndex];
  readings[readIndex] = newReading;
  total = total + newReading;
  readIndex = (readIndex + 1) % FILTER_SIZE;
  return total / FILTER_SIZE;
}

// ============================================
// IRRIGATION DECISION LOGIC
// ============================================
void irrigationDecision(int moisture) {
  if (moisture < THRESHOLD_DRY) {
    pumpShouldActivate = true;
  } else if (moisture > THRESHOLD_OPTIMAL_HIGH) {
    pumpShouldActivate = false;
  }
  // Hysteresis: maintain state between 30-70%
}

// ============================================
// UPDATE STATUS LEDS
// ============================================
void updateStatusLEDs(int moisture) {
  // Turn off all
  digitalWrite(LED_DRY, LOW);
  digitalWrite(LED_OPTIMAL, LOW);
  digitalWrite(LED_WET, LOW);
  
  // Turn on appropriate LED
  if (moisture < THRESHOLD_DRY) {
    digitalWrite(LED_DRY, HIGH);      // RED - DRY
  } else if (moisture < THRESHOLD_WET) {
    digitalWrite(LED_OPTIMAL, HIGH);  // YELLOW - GOOD
  } else {
    digitalWrite(LED_WET, HIGH);      // GREEN - WET
  }
}

// ============================================
// DISPLAY FUNCTION
// ============================================
void displayMoistureData(int raw, int filtered, int moisture) {
  Serial.println("┌────────────────────────────────────────┐");
  Serial.print("│ Moisture: ");
  Serial.print(moisture);
  Serial.print("%");
  
  // Add padding
  if (moisture < 10) Serial.print("  ");
  else if (moisture < 100) Serial.print(" ");
  
  // Status
  Serial.print(" [");
  if (moisture < 30) Serial.print("DRY    ");
  else if (moisture < 70) Serial.print("OPTIMAL");
  else Serial.print("WET    ");
  Serial.println("] │");
  
  Serial.print("│ Raw ADC:  ");
  Serial.print(raw);
  Serial.print(" → Filtered: ");
  Serial.print(filtered);
  
  // Padding
  int spaces = 9 - String(filtered).length();
  for (int i = 0; i < spaces; i++) Serial.print(" ");
  Serial.println("│");
  
  Serial.print("│ Pump:     ");
  if (pumpShouldActivate) {
    Serial.print("ON  ✓ (Irrigating)");
  } else {
    Serial.print("OFF ✗ (Standby)   ");
  }
  Serial.println("       │");
  
  Serial.println("└────────────────────────────────────────┘");
  Serial.println();
}

// ============================================
// HEADER FUNCTION
// ============================================
void printHeader() {
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║                                        ║");
  Serial.println("║         TAZROUT IRRIGATION             ║");
  Serial.println("║      Moisture Sensor Module            ║");
  Serial.println("║                                        ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║  Developer: KHENFRI Moussa             ║");
  Serial.println("║  Date: February 8, 2026                ║");
  Serial.println("║  Module: ESP32 Sensor Node             ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.println("Thresholds:");
  Serial.println("  • Dry (<30%):     Activate Pump");
  Serial.println("  • Optimal (30-70%): Monitor");
  Serial.println("  • Wet (>70%):     Stop Pump\n");
}
