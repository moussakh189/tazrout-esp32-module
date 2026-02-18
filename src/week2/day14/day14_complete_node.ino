/*
 * TAZROUT IoT Irrigation System
 * Day 14: Complete Sensor Node (Week 2 Final)
 * Developer: KHENFRI Moussa
 * Date: February 14, 2026
 */

#include <DHT.h>
#include <ArduinoJson.h>

// ── Forward Declarations  ──
void displayStatics();
void outputJson();

// ── Hardware ──────────────────────────────────

const int MOISTURE_PIN = 35;
const int DHT_PIN = 4;
#define DHT_TYPE DHT22
const int LED_DRY = 13;
const int LED_OPTIMAL = 12;
const int LED_WET = 14;

DHT dht(DHT_PIN, DHT_TYPE);

// ── Node ID ───────────────────────────────────
const char *NODE_ID = "TAZROUT_ESP32_001";

// ── Sensor Data Structure ─────────────────────
struct SensorReading
{
  int moisture;
  float temperature;
  float humidity;
  unsigned long timestamp;
  bool valid;
};

// ── Circular Buffer ───────────────────────────
const int BUFFER_SIZE = 10;
SensorReading dataBuffer[BUFFER_SIZE];
int bufferIndex = 0;
int bufferCount = 0;

// ── Current Reading ───────────────────────────
SensorReading current;

// ── System State ──────────────────────────────
bool pumpActive = false;
int pumpOnThreshold = 30;
unsigned long readingNumber = 0;

// ── Filter ────────────────────────────────────
const int FILTER_SIZE = 5;
int filterBuf[FILTER_SIZE];
int filterIndex = 0;
int filterTotal = 0;

// ── Calibration ───────────────────────────────
const int ADC_DRY = 0;
const int ADC_WET = 4095;

// ══════════════════════════════════════════════
void setup()
{
  Serial.begin(115200);
  delay(1500);

  pinMode(LED_DRY, OUTPUT);
  pinMode(LED_OPTIMAL, OUTPUT);
  pinMode(LED_WET, OUTPUT);

  dht.begin();
  delay(2000); // DHT stabilization

  for (int i = 0; i < FILTER_SIZE; i++)
    filterBuf[i] = 0;

  Serial.println("TAZROUT Complete Sensor Node Booted");
}

// ══════════════════════════════════════════════
void loop()
{

  readSensors();
  addToBuffer(current);
  adjustThreshold();
  irrigationDecision();
  updateLEDs();
  displayCompact();

  if (readingNumber % 10 == 0 && bufferCount >= 5)
  {
    displayStatistics();
  }

  if (readingNumber % 10 == 0)
  {
    outputJSON();
  }

  readingNumber++;
  delay(3000);
}

// ══════════════════════════════════════════════
void readSensors()
{

  int raw = analogRead(MOISTURE_PIN);
  int filtered = applyFilter(raw);

  current.moisture = map(filtered, ADC_DRY, ADC_WET, 0, 100);
  current.moisture = constrain(current.moisture, 0, 100);

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h))
  {
    delay(100);
    t = dht.readTemperature();
    h = dht.readHumidity();
  }

  current.temperature = t;
  current.humidity = h;
  current.valid = !isnan(t) && !isnan(h);
  current.timestamp = millis();
}

int applyFilter(int newVal)
{
  filterTotal -= filterBuf[filterIndex];
  filterBuf[filterIndex] = newVal;
  filterTotal += newVal;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;
  return filterTotal / FILTER_SIZE;
}

void addToBuffer(SensorReading reading)
{
  dataBuffer[bufferIndex] = reading;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
  if (bufferCount < BUFFER_SIZE)
    bufferCount++;
}

void adjustThreshold()
{
  if (!current.valid)
  {
    pumpOnThreshold = 30;
    return;
  }

  int adjustment = 0;

  if (current.temperature > 30)
    adjustment += 5;
  else if (current.temperature > 25)
    adjustment += 3;

  if (current.humidity < 40)
    adjustment += 5;
  else if (current.humidity < 60)
    adjustment += 2;

  pumpOnThreshold = constrain(30 + adjustment, 25, 50);
}

void irrigationDecision()
{
  if (current.moisture < pumpOnThreshold && !pumpActive)
  {
    pumpActive = true;
    Serial.println(">> PUMP ACTIVATED");
  }
  else if (current.moisture > 60 && pumpActive)
  {
    pumpActive = false;
    Serial.println(">> PUMP STOPPED");
  }
}

void updateLEDs()
{
  digitalWrite(LED_DRY, current.moisture < 30);
  digitalWrite(LED_OPTIMAL, current.moisture >= 30 && current.moisture < 70);
  digitalWrite(LED_WET, current.moisture >= 70);
}

void displayCompact()
{
  Serial.print("#");
  Serial.print(readingNumber);
  Serial.print(" M:");
  Serial.print(current.moisture);
  Serial.print("% ");

  if (current.valid)
  {
    Serial.print("T:");
    Serial.print(current.temperature, 1);
    Serial.print("C H:");
    Serial.print(current.humidity, 1);
    Serial.print("% ");
  }
  else
  {
    Serial.print("T:ERR H:ERR ");
  }

  Serial.print("Thr:");
  Serial.print(pumpOnThreshold);
  Serial.print(" Pump:");
  Serial.println(pumpActive ? "ON" : "OFF");
}

// ══════════════════════════════════════════════
// STATISTICS
// ══════════════════════════════════════════════
void displayStatistics()
{

  int moistureSum = 0, moistureMin = 100, moistureMax = 0;
  float tempSum = 0, tempMin = 999, tempMax = -999;
  float humSum = 0;
  int validCount = 0;

  for (int i = 0; i < bufferCount; i++)
  {
    moistureSum += dataBuffer[i].moisture;

    if (dataBuffer[i].moisture < moistureMin)
      moistureMin = dataBuffer[i].moisture;
    if (dataBuffer[i].moisture > moistureMax)
      moistureMax = dataBuffer[i].moisture;

    if (dataBuffer[i].valid)
    {
      validCount++;
      tempSum += dataBuffer[i].temperature;
      humSum += dataBuffer[i].humidity;

      if (dataBuffer[i].temperature < tempMin)
        tempMin = dataBuffer[i].temperature;
      if (dataBuffer[i].temperature > tempMax)
        tempMax = dataBuffer[i].temperature;
    }
  }

  Serial.println("\n========== STATISTICS ==========");
  Serial.print("Moisture Min:");
  Serial.print(moistureMin);
  Serial.print(" Avg:");
  Serial.print(moistureSum / bufferCount);
  Serial.print(" Max:");
  Serial.println(moistureMax);

  if (validCount > 0)
  {
    Serial.print("Temp Min:");
    Serial.print(tempMin, 1);
    Serial.print(" Avg:");
    Serial.print(tempSum / validCount, 1);
    Serial.print(" Max:");
    Serial.println(tempMax, 1);

    Serial.print("Humidity Avg:");
    Serial.println(humSum / validCount, 1);
  }

  Serial.println("===============================\n");
}

// ══════════════════════════════════════════════
// JSON OUTPUT
// ══════════════════════════════════════════════
void outputJSON()
{

  StaticJsonDocument<512> doc;

  doc["node_id"] = NODE_ID;
  doc["timestamp"] = current.timestamp;
  doc["reading_number"] = readingNumber;

  JsonObject sensors = doc.createNestedObject("sensors");

  JsonObject moisture = sensors.createNestedObject("moisture");
  moisture["value"] = current.moisture;
  moisture["valid"] = true;

  if (current.valid)
  {
    JsonObject temp = sensors.createNestedObject("temperature");
    temp["value"] = current.temperature;
    temp["unit"] = "celsius";

    JsonObject hum = sensors.createNestedObject("humidity");
    hum["value"] = current.humidity;
    hum["unit"] = "percent";
  }

  JsonObject irrigation = doc.createNestedObject("irrigation");
  irrigation["pump_active"] = pumpActive;
  irrigation["threshold"] = pumpOnThreshold;

  serializeJsonPretty(doc, Serial);
  Serial.println();
}
