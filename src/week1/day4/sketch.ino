/*
 * TAZROUT IoT Irrigation System - ESP32 Module
 * Day 4: Traffic Light State Machine - Part 2 (Enhanced)
 * 
 * Developer: KHENFRI Moussa
 * Date: February 4, 2026
 * Week: 1 - ESP32 Fundamentals
 * 
 * Features:
 * - Real-time countdown display
 * - State transition statistics
 * - State history tracking (circular buffer)
 * - Interactive serial commands
 * - Professional status reporting
 */

// Pin definitions
const int GREEN_LED = 14;
const int YELLOW_LED = 12;
const int RED_LED = 13;

// State definitions
enum TrafficLightState {
  STATE_GREEN,
  STATE_YELLOW,
  STATE_RED
};

// Current state
TrafficLightState currentState = STATE_GREEN;

// Timing variables
unsigned long stateStartTime = 0;
unsigned long stateDuration = 0;

// State durations
const unsigned long GREEN_DURATION = 5000;   // 5 seconds
const unsigned long YELLOW_DURATION = 2000;  // 2 seconds
const unsigned long RED_DURATION = 5000;     // 5 seconds

// Statistics tracking
unsigned long stateTransitions = 0;
unsigned long totalCycles = 0;

// State history (circular buffer)
const int HISTORY_SIZE = 10;
TrafficLightState stateHistory[HISTORY_SIZE];
int historyIndex = 0;

void setup() {
  // Initialize LED pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  
  // Initialize Serial
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("========================================");
  Serial.println("  TAZROUT - Traffic Light Enhanced    ");
  Serial.println("        Day 4: Professional Features   ");
  Serial.println("========================================");
  Serial.println("\nCommands: s=status, h=history, r=reset, ?=help\n");
  
  // Start in GREEN state
  changeState(STATE_GREEN);
}

void loop() {
  // Check for serial commands
  checkSerialCommands();
  
  // Update state machine
  updateStateMachine();
  
  // Display countdown every second
  static unsigned long lastCountdown = 0;
  if (millis() - lastCountdown >= 1000) {
    lastCountdown = millis();
    displayCountdown();
  }
  
  // Display detailed status every 10 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus >= 10000) {
    lastStatus = millis();
    printDetailedStatus();
  }
}

void changeState(TrafficLightState newState) {
  // Turn off all LEDs
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  
  // Update state
  currentState = newState;
  stateStartTime = millis();
  stateTransitions++;
  
  // Record in history
  recordStateInHistory(newState);
  
  // Check if completing a cycle
  if (newState == STATE_GREEN && stateTransitions > 1) {
    totalCycles++;
    Serial.print("\n*** CYCLE ");
    Serial.print(totalCycles);
    Serial.println(" COMPLETED ***\n");
  }
  
  // Configure new state
  switch (currentState) {
    case STATE_GREEN:
      digitalWrite(GREEN_LED, HIGH);
      stateDuration = GREEN_DURATION;
      Serial.println("\n>>> State Change: GREEN <<<");
      Serial.print("Duration: ");
      Serial.print(GREEN_DURATION / 1000.0);
      Serial.println(" seconds");
      break;
      
    case STATE_YELLOW:
      digitalWrite(YELLOW_LED, HIGH);
      stateDuration = YELLOW_DURATION;
      Serial.println("\n>>> State Change: YELLOW <<<");
      Serial.print("Duration: ");
      Serial.print(YELLOW_DURATION / 1000.0);
      Serial.println(" seconds");
      break;
      
    case STATE_RED:
      digitalWrite(RED_LED, HIGH);
      stateDuration = RED_DURATION;
      Serial.println("\n>>> State Change: RED <<<");
      Serial.print("Duration: ");
      Serial.print(RED_DURATION / 1000.0);
      Serial.println(" seconds");
      break;
  }
  
  Serial.print("Total transitions: ");
  Serial.println(stateTransitions);
  Serial.print("Completed cycles: ");
  Serial.println(totalCycles);
  Serial.println();
}

void updateStateMachine() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - stateStartTime >= stateDuration) {
    switch (currentState) {
      case STATE_GREEN:
        changeState(STATE_YELLOW);
        break;
      case STATE_YELLOW:
        changeState(STATE_RED);
        break;
      case STATE_RED:
        changeState(STATE_GREEN);
        break;
    }
  }
}

void displayCountdown() {
  unsigned long currentMillis = millis();
  unsigned long elapsedTime = currentMillis - stateStartTime;
  unsigned long remainingTime = stateDuration - elapsedTime;
  
  float remainingSeconds = remainingTime / 1000.0;
  
  Serial.print("Time remaining: ");
  Serial.print(remainingSeconds, 1);
  Serial.println("s");
}

void recordStateInHistory(TrafficLightState state) {
  stateHistory[historyIndex] = state;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

void printStateHistory() {
  Serial.println("\n--- Last 10 States ---");
  for (int i = 0; i < HISTORY_SIZE; i++) {
    int index = (historyIndex + i) % HISTORY_SIZE;
    Serial.print(i + 1);
    Serial.print(". ");
    
    switch (stateHistory[index]) {
      case STATE_GREEN:
        Serial.println("GREEN");
        break;
      case STATE_YELLOW:
        Serial.println("YELLOW");
        break;
      case STATE_RED:
        Serial.println("RED");
        break;
    }
  }
  Serial.println();
}

void printDetailedStatus() {
  Serial.println("\n======================================");
  Serial.println("    TRAFFIC LIGHT STATUS REPORT      ");
  Serial.println("======================================");
  
  // Current state
  Serial.print("Current State: ");
  switch (currentState) {
    case STATE_GREEN:
      Serial.println("GREEN");
      break;
    case STATE_YELLOW:
      Serial.println("YELLOW");
      break;
    case STATE_RED:
      Serial.println("RED");
      break;
  }
  
  // Timing info
  unsigned long elapsed = millis() - stateStartTime;
  unsigned long remaining = stateDuration - elapsed;
  
  Serial.print("Elapsed: ");
  Serial.print(elapsed / 1000.0, 1);
  Serial.println("s");
  
  Serial.print("Remaining: ");
  Serial.print(remaining / 1000.0, 1);
  Serial.println("s");
  
  // Statistics
  Serial.println("\n--- Statistics ---");
  Serial.print("Transitions: ");
  Serial.println(stateTransitions);
  Serial.print("Cycles: ");
  Serial.println(totalCycles);
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000.0, 1);
  Serial.println("s");
  
  // LED status
  Serial.println("\n--- LED Status ---");
  Serial.print("Green: ");
  Serial.println(digitalRead(GREEN_LED) ? "ON" : "OFF");
  Serial.print("Yellow: ");
  Serial.println(digitalRead(YELLOW_LED) ? "ON" : "OFF");
  Serial.print("Red: ");
  Serial.println(digitalRead(RED_LED) ? "ON" : "OFF");
  
  Serial.println("======================================\n");
}

void checkSerialCommands() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    
    switch (command) {
      case 's':
        printDetailedStatus();
        break;
        
      case 'h':
        printStateHistory();
        break;
        
      case 'r':
        totalCycles = 0;
        stateTransitions = 0;
        Serial.println("\n*** Statistics Reset ***\n");
        break;
        
      case '?':
        Serial.println("\n=== Available Commands ===");
        Serial.println("s - Show detailed status");
        Serial.println("h - Show state history");
        Serial.println("r - Reset statistics");
        Serial.println("? - Show this help");
        Serial.println("=========================\n");
        break;
        
      default:
        if (command != '\n' && command != '\r') {
          Serial.println("Unknown command. Type '?' for help.");
        }
        break;
    }
  }
}
