/*
 * TAZROUT IoT Irrigation System - ESP32 Module
 * Day 6: PWM LED Control with Potentiometer
 * 
 * Developer: KHENFRI Moussa
 * Date: February 6, 2026
 * Week: 1 - ESP32 Fundamentals
 * Project: TAZROUT IoT Irrigation System
 * 
 * Learning Objectives:
 * - Understand PWM (Pulse Width Modulation)
 * - Configure ESP32 LEDC (LED Control) peripheral
 * - Control LED brightness using analog input
 * - Implement smooth transitions
 * 
 * Hardware:
 * - ESP32 DevKit v1
 * - Potentiometer on GPIO 34 (input)
 * - LED on GPIO 26 (PWM output)
 * - 220 ohm resistor
 * 
 * TAZROUT Integration:
 * - PWM → Variable pump speed control (advanced)
 * - Smooth transitions → Gradual irrigation changes
 * - Analog control → Proportional response to sensors
 */

// Pin Definitions
const int POT_PIN = 34;   // Potentiometer input (ADC)
const int LED_PIN = 26;   // LED output (PWM)

// PWM Configuration
const int PWM_CHANNEL = 0;     // PWM channel (0-15)
const int PWM_FREQ = 5000;     // 5 kHz frequency
const int PWM_RESOLUTION = 8;  // 8-bit resolution (0-255)

// Moving average for smooth control
const int FILTER_SIZE = 5;
int readings[FILTER_SIZE];
int readIndex = 0;
int total = 0;

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("  TAZROUT ESP32 - PWM LED Control     ");
  Serial.println("         Day 6: Analog Output         ");
  Serial.println("========================================");
  Serial.println();
  
  // Configure PWM
  setupPWM();
  
  // Initialize filter
  for (int i = 0; i < FILTER_SIZE; i++) {
    readings[i] = 0;
  }
  
  Serial.println("PWM Configuration:");
  Serial.print("- Channel: ");
  Serial.println(PWM_CHANNEL);
  Serial.print("- Frequency: ");
  Serial.print(PWM_FREQ);
  Serial.println(" Hz");
  Serial.print("- Resolution: ");
  Serial.print(PWM_RESOLUTION);
  Serial.println("-bit (0-255)");
  Serial.println();
  Serial.println("Rotate potentiometer to control LED brightness!\n");
}

void loop() {
  // Read potentiometer (0-4095)
  int potValue = analogRead(POT_PIN);
  
  // Apply filter for smooth control
  int filtered = applyFilter(potValue);
  
  // Map to PWM range (0-255)
  int brightness = map(filtered, 0, 4095, 0, 255);
  brightness = constrain(brightness, 0, 255);
  
  // Set LED brightness
  ledcWrite(PWM_CHANNEL, brightness);
  
  // Calculate percentage
  int percentage = (brightness * 100) / 255;
  
  // Display values
  displayStatus(potValue, filtered, brightness, percentage);
  
  delay(100);
}

void setupPWM() {
  // Configure PWM channel
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  
  // Attach pin to channel
  ledcAttachPin(LED_PIN, PWM_CHANNEL);
  
  // Start with LED off
  ledcWrite(PWM_CHANNEL, 0);
  
  Serial.println("PWM initialized successfully!");
}

int applyFilter(int newValue) {
  // Remove oldest
  total = total - readings[readIndex];
  
  // Add new
  readings[readIndex] = newValue;
  total = total + newValue;
  
  // Advance index
  readIndex = (readIndex + 1) % FILTER_SIZE;
  
  // Return average
  return total / FILTER_SIZE;
}

void displayStatus(int raw, int filtered, int brightness, int percentage) {
  static int displayCounter = 0;
  displayCounter++;
  
  // Compact display every reading
  Serial.print("Pot: ");
  Serial.print(raw);
  Serial.print(" | PWM: ");
  Serial.print(brightness);
  Serial.print(" | Brightness: ");
  Serial.print(percentage);
  Serial.println("%");
  
  // Visual bar every 10 readings
  if (displayCounter % 10 == 0) {
    printBarGraph(percentage);
  }
  
  // Detailed info every 30 readings
  if (displayCounter % 30 == 0) {
    printDetailedInfo(raw, filtered, brightness, percentage);
  }
}

void printBarGraph(int percentage) {
  Serial.println();
  Serial.print("Brightness: [");
  
  int bars = percentage / 5;  // 20 bars for 100%
  for (int i = 0; i < 20; i++) {
    if (i < bars) {
      Serial.print("█");
    } else {
      Serial.print("░");
    }
  }
  
  Serial.print("] ");
  Serial.print(percentage);
  Serial.println("%");
  Serial.println();
}

void printDetailedInfo(int raw, int filtered, int brightness, int percentage) {
  Serial.println("========================================");
  Serial.println("       PWM DETAILED ANALYSIS           ");
  Serial.println("========================================");
  
  Serial.print("Input (ADC): ");
  Serial.print(raw);
  Serial.print(" / 4095 (");
  Serial.print((raw * 100) / 4095);
  Serial.println("%)");
  
  Serial.print("Filtered: ");
  Serial.println(filtered);
  
  Serial.print("PWM Duty Cycle: ");
  Serial.print(brightness);
  Serial.println(" / 255");
  
  Serial.print("LED Brightness: ");
  Serial.print(percentage);
  Serial.println("%");
  
  Serial.print("LED Status: ");
  if (brightness == 0) {
    Serial.println("OFF");
  } else if (brightness < 85) {
    Serial.println("DIM");
  } else if (brightness < 170) {
    Serial.println("MEDIUM");
  } else {
    Serial.println("BRIGHT");
  }
  
  Serial.println("========================================\n");
}
