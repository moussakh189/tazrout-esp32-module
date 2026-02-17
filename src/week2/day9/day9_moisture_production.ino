/*
 * TAZROUT IoT Irrigation System
 * Day 9: Moisture Sensor
 * Developer: KHENFRI Moussa
 * Date: February 9, 2026
 *
 * Pins:
 *  - Moisture sensor: GPIO 35
 *  - LED Red    (DRY)     : GPIO 13
 *  - LED Yellow (OPTIMAL) : GPIO 12
 *  - LED Green  (WET)     : GPIO 14
 *
 * How it works:
 *  - Potentiometer simulates the moisture sensor
 *  - Pot at MIN (left)  = Dry soil   → 0%
 *  - Pot at MAX (right) = Wet soil   → 100%
 *  - Fixed calibration values (no auto-calibration bug)
 */

// ── Pins ─────────────────────────────────────
const int MOISTURE_PIN = 35;
const int LED_DRY = 13;     // Red
const int LED_OPTIMAL = 12; // Yellow
const int LED_WET = 14;     // Green

// ── Calibration (Wokwi potentiometer range) ──
// Pot fully left  = ADC 0    → 0%  moisture (DRY)
// Pot fully right = ADC 4095 → 100% moisture (WET)
const int ADC_DRY = 0;
const int ADC_WET = 4095;

// ── Thresholds ───────────────────────────────
const int DRY_THRESHOLD = 30; // below 30% → pump ON
const int WET_THRESHOLD = 70; // above 70% → pump OFF

// ── Filter (moving average) ──────────────────
const int FILTER_SIZE = 5;
int filterBuf[FILTER_SIZE];
int filterIndex = 0;
int filterTotal = 0;
bool filterFull = false;

// ── State ────────────────────────────────────
bool pumpActive = false;

// ─────────────────────────────────────────────
void setup()
{
  Serial.begin(115200);

  pinMode(LED_DRY, OUTPUT);
  pinMode(LED_OPTIMAL, OUTPUT);
  pinMode(LED_WET, OUTPUT);

  // All LEDs off at start
  digitalWrite(LED_DRY, LOW);
  digitalWrite(LED_OPTIMAL, LOW);
  digitalWrite(LED_WET, LOW);

  // Init filter buffer
  for (int i = 0; i < FILTER_SIZE; i++)
    filterBuf[i] = 0;

  Serial.println("TAZROUT - Moisture Sensor Ready");
  Serial.println("Rotate the potentiometer to simulate soil moisture.");
  Serial.println("Left = Dry | Right = Wet");
  Serial.println("------------------------------------------");
}

// ─────────────────────────────────────────────
void loop()
{

  // 1. Read raw ADC
  int raw = analogRead(MOISTURE_PIN);

  // 2. Apply moving average filter
  int filtered = applyFilter(raw);

  // 3. Convert to percentage (0% = dry, 100% = wet)
  int moisture = map(filtered, ADC_DRY, ADC_WET, 0, 100);
  moisture = constrain(moisture, 0, 100);

  // 4. Pump decision with hysteresis
  if (moisture < DRY_THRESHOLD && !pumpActive)
  {
    pumpActive = true;
    Serial.println(">> PUMP ON  - soil is dry!");
  }
  else if (moisture > WET_THRESHOLD && pumpActive)
  {
    pumpActive = false;
    Serial.println(">> PUMP OFF - soil is wet enough.");
  }

  // 5. Update LEDs
  updateLEDs(moisture);

  // 6. Print to Serial
  printStatus(raw, filtered, moisture);

  delay(1000);
}

// ─────────────────────────────────────────────
int applyFilter(int newVal)
{
  filterTotal -= filterBuf[filterIndex];
  filterBuf[filterIndex] = newVal;
  filterTotal += newVal;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;
  return filterTotal / FILTER_SIZE;
}

// ─────────────────────────────────────────────
void updateLEDs(int moisture)
{
  // Turn all off first
  digitalWrite(LED_DRY, LOW);
  digitalWrite(LED_OPTIMAL, LOW);
  digitalWrite(LED_WET, LOW);

  // Turn on the right one
  if (moisture < DRY_THRESHOLD)
  {
    digitalWrite(LED_DRY, HIGH); // Red   - needs water
  }
  else if (moisture <= WET_THRESHOLD)
  {
    digitalWrite(LED_OPTIMAL, HIGH); // Yellow - good
  }
  else
  {
    digitalWrite(LED_WET, HIGH); // Green  - too wet
  }
}

// ─────────────────────────────────────────────
void printStatus(int raw, int filtered, int moisture)
{
  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print("  |  Filtered: ");
  Serial.print(filtered);
  Serial.print("  |  Moisture: ");
  Serial.print(moisture);
  Serial.print("%  |  Status: ");

  if (moisture < DRY_THRESHOLD)
  {
    Serial.print("DRY");
  }
  else if (moisture <= WET_THRESHOLD)
  {
    Serial.print("OPTIMAL");
  }
  else
  {
    Serial.print("WET");
  }

  Serial.print("  |  Pump: ");
  Serial.println(pumpActive ? "ON" : "OFF");
}
