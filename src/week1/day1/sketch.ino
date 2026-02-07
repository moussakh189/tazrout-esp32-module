/*
 * TAZROUT IoT Irrigation System - ESP32 Module
 * Day 1: LED Blink - Environment Setup
 * 
 * Developer: KHENFRI Moussa
 * Date: February 1, 2026
 * Week: 1 - ESP32 Fundamentals
 * Project: TAZROUT IoT Irrigation System
 * 
 * Learning Objectives:
 * - Understand ESP32 GPIO control
 * - Master digital output operations
 * - Verify Wokwi simulation environment
 * 
 * Hardware:
 * - ESP32 DevKit v1
 * - LED connected to GPIO 2
 * - 220 ohm resistor
 * 
 * TAZROUT Integration:
 * - GPIO control → Future relay control for irrigation pump
 * - LED patterns → System status indicators
 */

// Pin Definitions
const int LED_PIN = 2;  // Built-in LED pin on most ESP32 boards

// Timing Configuration
const int BLINK_INTERVAL = 500;  // 500ms = 1Hz frequency (on + off = 1 second)

void setup() {
  // Initialize the LED pin as an output
  pinMode(LED_PIN, OUTPUT);
  
  // Initialize Serial communication for debugging
  Serial.begin(115200);
  delay(1000);  // Give Serial time to initialize
  
  Serial.println("=================================");
  Serial.println("TAZROUT - Day 1: LED Blink Test");
  Serial.println("=================================");
  Serial.println("LED Pin: GPIO 2");
  Serial.println("Frequency: 1Hz (500ms ON, 500ms OFF)");
  Serial.println("=================================\n");
}

void loop() {
  // Turn LED ON
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED: ON");
  delay(BLINK_INTERVAL);
  
  // Turn LED OFF
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED: OFF");
  delay(BLINK_INTERVAL);
}
