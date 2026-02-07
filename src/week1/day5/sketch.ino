/*
 * TAZROUT IoT Irrigation System - ESP32 Module
 * Day 5: Analog Input - Potentiometer Reading
 *
 * Developer: KHENFRI Moussa
 * Date: February 5, 2026
 * Week: 1 - ESP32 Fundamentals
 * Project: TAZROUT IoT Irrigation System
 *
 * Learning Objectives:
 * - Read analog values using ADC (Analog-to-Digital Converter)
 * - Understand ESP32 ADC characteristics (12-bit, 0-4095 range)
 * - Implement sensor calibration
 * - Apply moving average filter for noise reduction
 * - Map analog values to meaningful percentages
 *
 * Hardware:
 * - ESP32 DevKit v1
 * - Potentiometer connected to GPIO 34 (ADC1_CH6)
 *
 * TAZROUT Integration:
 * - ADC reading → Moisture sensor (Week 2)
 * - Calibration → Sensor accuracy (Week 2)
 * - Moving average → Noise filtering for all sensors
 * - Mapping → Convert raw ADC to moisture percentage
 */

// Pin Definitions
const int POT_PIN = 34;  // GPIO 34 - ADC1 Channel 6

// Calibration values (will be set during calibration)
int adcMin = 0;     // Minimum ADC reading (dry)
int adcMax = 4095;  // Maximum ADC reading (wet)

// Moving average filter
const int FILTER_SIZE = 5;
int readings[FILTER_SIZE];
int readIndex = 0;
int total = 0;
int average = 0;

// Update interval
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 500;  // 500ms

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  delay(1000);

  // Print startup banner
  Serial.println("\n========================================");
  Serial.println("  TAZROUT ESP32 - Analog Input Demo   ");
  Serial.println("         Day 5: Potentiometer          ");
  Serial.println("========================================");
  Serial.println();
  Serial.println("ESP32 ADC Characteristics:");
  Serial.println("- Resolution: 12-bit (0-4095)");
  Serial.println("- Pin: GPIO 34 (ADC1_CH6)");
  Serial.println("- Voltage Range: 0-3.3V");
  Serial.println("========================================\n");

  // Initialize moving average filter
  for (int i = 0; i < FILTER_SIZE; i++) {
    readings[i] = 0;
  }

  // Perform calibration
  calibrateSensor();

  Serial.println("\n>>> Starting continuous reading <<<\n");
}

void loop() {
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();

    // Read raw ADC value
    int rawValue = analogRead(POT_PIN);

    // Apply moving average filter
    int filteredValue = applyMovingAverage(rawValue);

    // Map to percentage (0-100%)
    int percentage = map(filteredValue, adcMin, adcMax, 0, 100);
    percentage = constrain(percentage, 0, 100);

    // Display all values
    displayReadings(rawValue, filteredValue, percentage);
  }
}

void calibrateSensor() {
  Serial.println(">>> CALIBRATION MODE <<<");
  Serial.println("Calibrating sensor...");
  Serial.println("Rotating potentiometer through full range...\n");

  // Sample for 3 seconds to find min and max
  unsigned long calibrationStart = millis();
  int tempMin = 4095;
  int tempMax = 0;

  while (millis() - calibrationStart < 3000) {
    int reading = analogRead(POT_PIN);

    if (reading < tempMin) tempMin = reading;
    if (reading > tempMax) tempMax = reading;

    delay(10);
  }

  // Set calibration values with some margin
  adcMin = tempMin - 10;
  adcMax = tempMax + 10;

  // Ensure within valid range
  adcMin = constrain(adcMin, 0, 4095);
  adcMax = constrain(adcMax, 0, 4095);

  Serial.println("Calibration Complete!");
  Serial.print("ADC Min: ");
  Serial.println(adcMin);
  Serial.print("ADC Max: ");
  Serial.println(adcMax);
  Serial.print("Range: ");
  Serial.println(adcMax - adcMin);

  // Verify calibration
  if (adcMax - adcMin < 100) {
    Serial.println("\n[WARNING] Small range detected!");
    Serial.println("Please rotate potentiometer fully and restart.");
  }
}

int applyMovingAverage(int newReading) {
  // Subtract oldest reading
  total = total - readings[readIndex];

  // Add new reading
  readings[readIndex] = newReading;
  total = total + newReading;

  // Advance to next position
  readIndex = (readIndex + 1) % FILTER_SIZE;

  // Calculate average
  average = total / FILTER_SIZE;

  return average;
}

void displayReadings(int raw, int filtered, int percentage) {
  // Display in multiple formats

  // 1. Compact format
  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print(" | Filtered: ");
  Serial.print(filtered);
  Serial.print(" | Percentage: ");
  Serial.print(percentage);
  Serial.println("%");

  // 2. Visual bar graph (every 5 readings)
  static int readingCount = 0;
  readingCount++;

  if (readingCount % 10 == 0) {
    Serial.println();
    Serial.println("Visual Representation:");
    Serial.print("[");

    int bars = percentage / 5;  // 20 bars max (100/5)
    for (int i = 0; i < 20; i++) {
      if (i < bars) {
        Serial.print("=");
      } else {
        Serial.print(" ");
      }
    }
    Serial.print("] ");
    Serial.print(percentage);
    Serial.println("%");
    Serial.println();
  }

  // 3. Detailed format (every 20 readings)
  if (readingCount % 20 == 0) {
    printDetailedAnalysis(raw, filtered, percentage);
  }
}

void printDetailedAnalysis(int raw, int filtered, int percentage) {
  Serial.println("\n========================================");
  Serial.println("       DETAILED SENSOR ANALYSIS        ");
  Serial.println("========================================");

  // Raw value
  Serial.print("Raw ADC Value: ");
  Serial.print(raw);
  Serial.print(" (0x");
  Serial.print(raw, HEX);
  Serial.println(")");

  // Filtered value
  Serial.print("Filtered Value: ");
  Serial.print(filtered);
  Serial.print(" (noise reduction: ");
  Serial.print(abs(raw - filtered));
  Serial.println(")");

  // Voltage calculation
  float voltage = (filtered / 4095.0) * 3.3;
  Serial.print("Estimated Voltage: ");
  Serial.print(voltage, 2);
  Serial.println("V");

  // Percentage
  Serial.print("Mapped Percentage: ");
  Serial.print(percentage);
  Serial.println("%");

  // Status interpretation
  Serial.print("Status: ");
  if (percentage < 30) {
    Serial.println("LOW (Dry)");
  } else if (percentage < 70) {
    Serial.println("MEDIUM (Optimal)");
  } else {
    Serial.println("HIGH (Wet)");
  }

  // Calibration info
  Serial.println("\nCalibration Range:");
  Serial.print("  Min: ");
  Serial.print(adcMin);
  Serial.print(" | Max: ");
  Serial.println(adcMax);

  Serial.println("========================================\n");
}
