/*
 * TAZROUT IoT Irrigation System - ESP32 Module
 * Day 3: Traffic Light State Machine - Part 1
 * 
 * Developer: KHENFRI Moussa
 * Date: February 3, 2026
 * Week: 1 - ESP32 Fundamentals
 * Project: TAZROUT IoT Irrigation System
 * 
 * Learning Objectives:
 * - Understand and implement state machine design pattern
 * - Use millis() for non-blocking timing
 * - Control multiple outputs independently
 * - Implement smooth state transitions
 * 
 * Hardware:
 * - ESP32 DevKit v1
 * - Green LED on GPIO 14
 * - Yellow LED on GPIO 12
 * - Red LED on GPIO 13
 * - 3x 220 ohm resistors
 * 
 * TAZROUT Integration:
 * - State machine → Irrigation states (IDLE, EVALUATING, WATERING, COOLDOWN)
 * - Non-blocking timing → Read sensors while controlling pump
 * - Pattern directly applicable to Week 5
 */

// Pin definitions
const int GREEN_LED = 14;
const int YELLOW_LED = 12;
const int RED_LED = 13;

// Define states using enum
enum TrafficLightState {
  STATE_GREEN,
  STATE_YELLOW,
  STATE_RED
};

// Current state variable
TrafficLightState currentState = STATE_GREEN;

// Timing variables
unsigned long previousMillis = 0;
unsigned long stateDuration = 0;

// State durations (in milliseconds)
const unsigned long GREEN_DURATION = 5000;   // 5 seconds
const unsigned long YELLOW_DURATION = 2000;  // 2 seconds
const unsigned long RED_DURATION = 5000;     // 5 seconds

void setup() {
  // Initialize LED pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  
  // Initialize Serial
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("====================================");
  Serial.println("  TAZROUT - Traffic Light System  ");
  Serial.println("        Day 3: State Machine       ");
  Serial.println("====================================\n");
  
  // Start in GREEN state
  changeState(STATE_GREEN);
}

void loop() {
  // Update state machine
  updateStateMachine();
  
  // Loop runs continuously - not blocked by delays!
  // Could add sensor reading here (Week 2)
  // Could check WiFi here (Week 3)
  // Could process MQTT here (Week 4)
}

void changeState(TrafficLightState newState) {
  // Turn off all LEDs first
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  
  // Update current state
  currentState = newState;
  previousMillis = millis();  // Reset timer
  
  // Turn on appropriate LED and set duration
  switch (currentState) {
    case STATE_GREEN:
      digitalWrite(GREEN_LED, HIGH);
      stateDuration = GREEN_DURATION;
      Serial.println("State: GREEN (5s)");
      break;
      
    case STATE_YELLOW:
      digitalWrite(YELLOW_LED, HIGH);
      stateDuration = YELLOW_DURATION;
      Serial.println("State: YELLOW (2s)");
      break;
      
    case STATE_RED:
      digitalWrite(RED_LED, HIGH);
      stateDuration = RED_DURATION;
      Serial.println("State: RED (5s)");
      break;
  }
}

void updateStateMachine() {
  unsigned long currentMillis = millis();
  
  // Check if enough time has passed in current state
  if (currentMillis - previousMillis >= stateDuration) {
    // Time to change state!
    
    switch (currentState) {
      case STATE_GREEN:
        changeState(STATE_YELLOW);  // GREEN -> YELLOW
        break;
        
      case STATE_YELLOW:
        changeState(STATE_RED);     // YELLOW -> RED
        break;
        
      case STATE_RED:
        changeState(STATE_GREEN);   // RED -> GREEN
        break;
    }
  }
}
