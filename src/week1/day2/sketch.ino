/*
 * TAZROUT IoT Irrigation System - ESP32 Module
 * Day 2: Serial Communication Patterns
 * 
 * Developer: KHENFRI Moussa
 * Date: February 2, 2026
 * Week: 1 - ESP32 Fundamentals
 * Project: TAZROUT IoT Irrigation System
 * 
 * Learning Objectives:
 * - Master Serial communication and formatting
 * - Practice different output formats (text, table, JSON, debug)
 * - Understand data type formatting
 * - Preview JSON structure for Week 3 MQTT
 * 
 * TAZROUT Integration:
 * - JSON format → MQTT message payloads (Week 3)
 * - Debug logging → System monitoring (Week 6)
 * - Data formatting → Sensor value display (Week 2)
 */

// Simulated sensor values
float temperature = 24.5;
int moisture = 65;
float humidity = 58.3;
bool pumpStatus = false;

// System tracking
unsigned long readingCount = 0;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 5000;  // Update every 5 seconds

void setup() {
  // Initialize Serial at ESP32 standard baud rate
  Serial.begin(115200);
  delay(1000);  // Wait for Serial to stabilize
  
  // Print startup banner
  printStartupBanner();
  
  // Demonstrate different data types
  demonstrateDataTypes();
  
  // Show system information
  printSystemInfo();
  
  Serial.println("\n=== Starting sensor simulation ===\n");
}

void loop() {
  // Update readings every 5 seconds
  if (millis() - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = millis();
    
    // Update simulated sensor values
    updateSimulatedReadings();
    
    // Print in different formats (rotate through them)
    int formatType = (readingCount % 4);
    
    switch(formatType) {
      case 0:
        printBasicFormat();
        break;
      case 1:
        printTableFormat();
        break;
      case 2:
        printJSONFormat();
        break;
      case 3:
        printDebugFormat();
        break;
    }
    
    Serial.println("\n--------------------------------------------------\n");
  }
}

void printStartupBanner() {
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  TAZROUT ESP32 - Serial Communication ");
  Serial.println("           Day 2 Demo                  ");
  Serial.println("========================================");
  Serial.println();
  Serial.println("Device: ESP32 DevKit v1");
  Serial.println("Baud Rate: 115200");
  Serial.println("Developer: KHENFRI Moussa");
  Serial.println("========================================");
  Serial.println();
}

void demonstrateDataTypes() {
  Serial.println(">>> DATA TYPE FORMATTING <<<\n");
  
  int value = 1023;
  Serial.print("Integer (decimal): ");
  Serial.println(value);
  Serial.print("Integer (hex): 0x");
  Serial.println(value, HEX);
  Serial.print("Integer (binary): 0b");
  Serial.println(value, BIN);
  
  float temp = 24.56789;
  Serial.print("Float (default): ");
  Serial.println(temp);
  Serial.print("Float (1 decimal): ");
  Serial.println(temp, 1);
  Serial.print("Float (3 decimals): ");
  Serial.println(temp, 3);
  
  bool status = true;
  Serial.print("Boolean: ");
  Serial.println(status ? "true" : "false");
  
  String id = "ESP32_001";
  Serial.print("String: ");
  Serial.println(id);
  
  Serial.println();
}

void updateSimulatedReadings() {
  temperature += random(-10, 10) / 10.0;
  temperature = constrain(temperature, 20.0, 30.0);
  
  moisture += random(-5, 5);
  moisture = constrain(moisture, 50, 80);
  
  humidity += random(-20, 20) / 10.0;
  humidity = constrain(humidity, 40.0, 80.0);
  
  pumpStatus = (moisture < 60);
  readingCount++;
}

void printBasicFormat() {
  Serial.println(">>> BASIC FORMAT <<<");
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" C");
  Serial.print("Moisture: ");
  Serial.print(moisture);
  Serial.println(" %");
  Serial.print("Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");
  Serial.print("Pump: ");
  Serial.println(pumpStatus ? "ON" : "OFF");
}

void printTableFormat() {
  Serial.println(">>> TABLE FORMAT <<<");
  Serial.println("+-------------+----------+------+");
  Serial.println("|   Sensor    |  Value   | Unit |");
  Serial.println("+-------------+----------+------+");
  Serial.print("| Temperature | ");
  Serial.print(temperature, 1);
  Serial.println("   |  C   |");
  Serial.print("| Moisture    | ");
  Serial.print(moisture);
  Serial.println("      |  %   |");
  Serial.print("| Humidity    | ");
  Serial.print(humidity, 1);
  Serial.println("   |  %   |");
  Serial.println("+-------------+----------+------+");
}

void printJSONFormat() {
  Serial.println(">>> JSON FORMAT <<<");
  Serial.println("{");
  Serial.print("  \"node_id\": \"ESP32_001\",\n");
  Serial.print("  \"reading\": ");
  Serial.print(readingCount);
  Serial.println(",");
  Serial.print("  \"timestamp\": ");
  Serial.print(millis());
  Serial.println(",");
  Serial.print("  \"temperature\": ");
  Serial.print(temperature, 1);
  Serial.println(",");
  Serial.print("  \"moisture\": ");
  Serial.print(moisture);
  Serial.println(",");
  Serial.print("  \"humidity\": ");
  Serial.print(humidity, 1);
  Serial.println(",");
  Serial.print("  \"pump_status\": ");
  Serial.println(pumpStatus ? "true" : "false");
  Serial.println("}");
}

void printDebugFormat() {
  Serial.println(">>> DEBUG FORMAT <<<");
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] Reading #");
  Serial.println(readingCount);
  
  Serial.print("[INFO] Temp=");
  Serial.print(temperature, 1);
  Serial.print("C, Moisture=");
  Serial.print(moisture);
  Serial.print("%, Humidity=");
  Serial.print(humidity, 1);
  Serial.println("%");
  
  if (pumpStatus) {
    Serial.println("[ALERT] Pump ON - low moisture!");
  } else {
    Serial.println("[STATUS] Moisture OK, pump OFF");
  }
  
  Serial.print("[SYSTEM] Free heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
}

void printSystemInfo() {
  Serial.println(">>> ESP32 SYSTEM INFO <<<");
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");
  Serial.print("CPU Frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.println();
}
