

#include <DHT.h>
#include <ArduinoJson.h>
#include <driver/adc.h>
#include <esp_system.h>

// ── Firmware Identity ─────────────────────────────────────────────────
#define FW_VERSION       "3.1.0"
#define PROTOCOL_VERSION  0x01
#define DEVICE_ID        "MCU-ZONE-A-001"
#define ZONE_ID          "zone_a"
#define ZONE_NAME        "Zone A"

// ── Pins  ──────────────────────────────────────────────────
#define PIN_DHT           16
#define PIN_VALVE         26
#define PIN_LED_STATUS     2
#define PIN_LED_DRY       13
#define PIN_LED_OPTIMAL   12
#define PIN_LED_WET       14
#define DHT_TYPE DHT22

// ── ADC channels ──────────────────────────────────────────────────────
#define MOISTURE_CH   ADC1_CHANNEL_7    // GPIO 35
#define WATER_CH      ADC1_CHANNEL_6    // GPIO 34

// ── Calibration — measure with YOUR sensor before first use ───────────
// Capacitive sensor output is INVERTED:
//   Dry in air  → high ADC (~2800)  → 0% moisture
//   In water    → low ADC  (~1200)  → 100% moisture
#define ADC_MOISTURE_DRY   2800
#define ADC_MOISTURE_WET   1200
#define ADC_WATER_EMPTY     400
#define ADC_WATER_FULL     3600

// ── Timing ────────────────────────────────────────────────────────────
#define SENSOR_INTERVAL_MS   30000UL
#define HEARTBEAT_INTERVAL_MS  500UL
#define DHT_RETRY_DELAY_MS     500UL

// ── Filter ────────────────────────────────────────────────────────────
#define FILTER_SIZE 10

// ── Alert thresholds  ──────────────────────────────────────
#define ALERT_TEMP_HIGH_C         45.0f
#define ALERT_TEMP_LOW_C           5.0f
#define ALERT_MOISTURE_LOW_GM3   150.0f
#define ALERT_WATER_LOW_PCT       10.0f
#define ALERT_HUMIDITY_LOW_PCT    20.0f

// ── Valve safety ──────────────────────────────────────────
#define VALVE_MAX_DURATION_MIN   60
#define VALVE_WATCHDOG_MIN       65
#define VALVE_MIN_WATER_PCT       5.0f

// ── Packet type bytes  ─────────────────────────────────────
#define PKT_SENSOR_READING   0x01
#define PKT_COMMAND_ACK      0x02
#define PKT_DEVICE_STATE     0x03
#define PKT_EMERGENCY_ALERT  0x04

// ── Data structures ───────────────────────────────────────────────────

struct SensorReading {
  float temperature_c;
  float humidity_pct;
  float soil_moisture_gm3;
  float water_level_pct;
  bool  dht_valid;
  bool  moisture_valid;
  bool  water_valid;
  unsigned long timestamp_ms;
};

struct ValveState {
  bool     open;
  unsigned long opened_at_ms;
  unsigned long duration_ms;
  bool     timer_active;
  String   last_command_id;
  uint32_t total_cycles;
  bool     safety_triggered;
};

enum SensorStatus { S_OK, S_OUT_OF_RANGE, S_FAULT };

struct Filter {
  int  buf[FILTER_SIZE];
  int  idx;
  long total;
};

// ── Globals ───────────────────────────────────────────────────────────

DHT           dht(PIN_DHT, DHT_TYPE);
SensorReading current;
ValveState    valve;
Filter        moistFilter, waterFilter;

unsigned long lastSensorMs    = 0;
unsigned long lastHeartbeatMs = 0;
bool          heartbeat       = false;
uint32_t      alertCounter    = 0;

// Alert debounce state
bool   alertActive      = false;
String lastAlertMessage = "";

// ── Forward declarations ───────────────────────────────────────────────
void readAllSensors();
int  filterUpdate(Filter& f, int val);
void filterWarmUp(Filter& f, adc1_channel_t ch);
// TO DO
void sendSensorReading();
void sendCommandAck(const String& id, const String& status,
                    const String& vState, const String& msg);
void sendDeviceStateChange(const String& prev, const String& curr,
                           const String& reason = "");
void sendEmergencyAlert(const String& trigger, const String& msg,
                        const String& action);
void checkAlertThresholds();   // FIX #4 — was missing

void openValve(unsigned long durMs, const String& cmdId);
void closeValve(const String& cmdId, const String& reason);
void tickValveTimer();
void executeValveCommand(const char* json);

void transmitPacket(uint8_t type, const char* payload);
bool receivePacket(char* out, size_t maxLen);
uint16_t crc16(const uint8_t* data, size_t len);

SensorStatus tempStatus();
SensorStatus humidStatus();
SensorStatus moistStatus();
SensorStatus waterStatus();
const char*  statusStr(SensorStatus s);

void updateLEDs();
void tickHeartbeat();
void processSerial();
void printBanner();

// ═══════════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_VALVE,       OUTPUT);
  pinMode(PIN_LED_STATUS,  OUTPUT);
  pinMode(PIN_LED_DRY,     OUTPUT);
  pinMode(PIN_LED_OPTIMAL, OUTPUT);
  pinMode(PIN_LED_WET,     OUTPUT);

  digitalWrite(PIN_VALVE,       LOW);
  digitalWrite(PIN_LED_STATUS,  LOW);
  digitalWrite(PIN_LED_DRY,     LOW);
  digitalWrite(PIN_LED_OPTIMAL, LOW);
  digitalWrite(PIN_LED_WET,     LOW);

  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(MOISTURE_CH, ADC_ATTEN_DB_12);
  adc1_config_channel_atten(WATER_CH,    ADC_ATTEN_DB_12);

  dht.begin();

  filterWarmUp(moistFilter, MOISTURE_CH);
  filterWarmUp(waterFilter,  WATER_CH);

  memset(&valve, 0, sizeof(valve));
  delay(2000);

  printBanner();
  sendDeviceStateChange("OFFLINE", "ONLINE");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorMs >= SENSOR_INTERVAL_MS) {
    lastSensorMs = now;
    readAllSensors();
    updateLEDs();
    sendSensorReading();
    checkAlertThresholds();
  }

  tickValveTimer();
  processSerial();

  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    heartbeat = !heartbeat;
    digitalWrite(PIN_LED_STATUS, valve.open ? HIGH : (heartbeat ? HIGH : LOW));
  }
}

//  SENSOR READING

void readAllSensors() {
  // DHT22
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    delay(DHT_RETRY_DELAY_MS);
    t = dht.readTemperature();
    h = dht.readHumidity();
  }
  current.dht_valid     = !isnan(t) && !isnan(h);
  current.temperature_c = current.dht_valid ? t : -999.0f;
  current.humidity_pct  = current.dht_valid ? h : -999.0f;

  // Soil moisture — capacitive, INVERTED mapping
  int rawM  = adc1_get_raw(MOISTURE_CH);
  int filtM = filterUpdate(moistFilter, rawM);
  int pctM  = constrain((int)map(filtM,
                ADC_MOISTURE_DRY, ADC_MOISTURE_WET, 0, 100), 0, 100);
  current.soil_moisture_gm3 = pctM * 20.0f;
  current.moisture_valid    = (filtM > 50 && filtM < 4050);

  // Water tank level
  int rawW  = adc1_get_raw(WATER_CH);
  int filtW = filterUpdate(waterFilter, rawW);
  int pctW  = constrain((int)map(filtW,
                ADC_WATER_EMPTY, ADC_WATER_FULL, 0, 100), 0, 100);
  current.water_level_pct = (float)pctW;
  current.water_valid     = (filtW > 50 && filtW < 4050);

  current.timestamp_ms = millis();
}

//  FILTER — FIX #3: filterWarmUp takes adc1_channel_t, not int pin

int filterUpdate(Filter& f, int val) {
  f.total    -= f.buf[f.idx];
  f.buf[f.idx] = val;
  f.total    += val;
  f.idx       = (f.idx + 1) % FILTER_SIZE;
  return (int)(f.total / FILTER_SIZE);
}

void filterWarmUp(Filter& f, adc1_channel_t ch) {
  memset(&f, 0, sizeof(f));
  for (int i = 0; i < FILTER_SIZE; i++) {
    f.buf[i] = adc1_get_raw(ch);
    f.total += f.buf[i];
    delay(5);
  }
}

SensorStatus tempStatus() {
  if (!current.dht_valid) return S_FAULT;
  if (current.temperature_c < -40 || current.temperature_c > 80) return S_OUT_OF_RANGE;
  return S_OK;
}
SensorStatus humidStatus() {
  if (!current.dht_valid) return S_FAULT;
  if (current.humidity_pct < 0 || current.humidity_pct > 100) return S_OUT_OF_RANGE;
  return S_OK;
}
SensorStatus moistStatus() {
  if (!current.moisture_valid) return S_FAULT;
  return S_OK;
}
SensorStatus waterStatus() {
  if (!current.water_valid) return S_FAULT;
  return S_OK;
}
const char* statusStr(SensorStatus s) {
  switch (s) {
    case S_OK:           return "OK";
    case S_OUT_OF_RANGE: return "OUT_OF_RANGE";
    case S_FAULT:        return "SENSOR_FAULT";
    default:             return "UNKNOWN";
  }
}


//  PACKET 1 — SENSOR_READING

void sendSensorReading() {
  StaticJsonDocument<512> doc;
  doc["packet_type"] = "SENSOR_READING";
  doc["zone_id"]     = ZONE_ID;
  doc["zone_name"]   = ZONE_NAME;
  doc["device_id"]   = DEVICE_ID;
  doc["uptime_ms"]   = current.timestamp_ms;

  JsonObject s = doc.createNestedObject("sensors");

  JsonObject temp  = s.createNestedObject("temperature");
  temp["value"]  = serialized(String(current.temperature_c, 1));
  temp["unit"]   = "C";
  temp["status"] = statusStr(tempStatus());

  JsonObject moist = s.createNestedObject("soil_moisture");
  moist["value"]  = serialized(String(current.soil_moisture_gm3, 1));
  moist["unit"]   = "g/m3";
  moist["status"] = statusStr(moistStatus());

  JsonObject water = s.createNestedObject("water_level");
  water["value"]  = serialized(String(current.water_level_pct, 1));
  water["unit"]   = "%";
  water["status"] = statusStr(waterStatus());

  JsonObject hum   = s.createNestedObject("humidity");
  hum["value"]  = serialized(String(current.humidity_pct, 1));
  hum["unit"]   = "%";
  hum["status"] = statusStr(humidStatus());

  JsonObject sig = doc.createNestedObject("signal");
  sig["rssi"] = 0; sig["snr"] = 0; sig["spreading_factor"] = 7;

  char buf[512];
  serializeJson(doc, buf, sizeof(buf));
  transmitPacket(PKT_SENSOR_READING, buf);
}

//  PACKET 2 — COMMAND_ACK

void sendCommandAck(const String& id, const String& status,
                    const String& vState, const String& msg) {
  StaticJsonDocument<384> doc;
  doc["packet_type"]       = "COMMAND_ACK";
  doc["command_id"]        = id;
  doc["zone_id"]           = ZONE_ID;
  doc["device_id"]         = DEVICE_ID;
  doc["uptime_ms"]         = millis();
  doc["status"]            = status;
  doc["valve_state_after"] = vState;
  doc["message"]           = msg;
  char buf[384];
  serializeJson(doc, buf, sizeof(buf));
  transmitPacket(PKT_COMMAND_ACK, buf);
}

//  PACKET 3 — DEVICE_STATE_CHANGE
void sendDeviceStateChange(const String& prev, const String& curr,
                           const String& reason) {
  StaticJsonDocument<300> doc;
  doc["packet_type"]           = "DEVICE_STATE_CHANGE";
  doc["zone_id"]               = ZONE_ID;
  doc["device_id"]             = DEVICE_ID;
  doc["uptime_ms"]             = millis();
  doc["previous_device_state"] = prev;
  doc["current_device_state"]  = curr;
  doc["valve_state"]           = valve.open ? "OPEN" : "CLOSED";
  doc["firmware_version"]      = FW_VERSION;
  doc["battery_level"]         = -1;
  if (reason.length() > 0) doc["reason"] = reason;
  char buf[300];
  serializeJson(doc, buf, sizeof(buf));
  transmitPacket(PKT_DEVICE_STATE, buf);
}


//  PACKET 4 — EMERGENCY_ALERT

void sendEmergencyAlert(const String& trigger, const String& msg,
                        const String& action) {
  StaticJsonDocument<512> doc;
  char alertId[24];
  snprintf(alertId, sizeof(alertId), "ALERT-%05lu", ++alertCounter);
  doc["packet_type"]        = "EMERGENCY_ALERT";
  doc["alert_id"]           = alertId;
  doc["uptime_ms"]          = millis();
  doc["severity"]           = "CRITICAL";
  doc["issued_by"]          = DEVICE_ID;
  doc["zone_id"]            = ZONE_ID;
  doc["triggered_by"]       = trigger;
  JsonObject sv = doc.createNestedObject("sensor_values");
  sv["temperature"]   = current.dht_valid ? current.temperature_c : -999.0f;
  sv["soil_moisture"] = current.soil_moisture_gm3;
  sv["water_level"]   = current.water_level_pct;
  sv["humidity"]      = current.dht_valid ? current.humidity_pct : -999.0f;
  doc["message"]            = msg;
  doc["recommended_action"] = action;
  char buf[512];
  serializeJson(doc, buf, sizeof(buf));
  transmitPacket(PKT_EMERGENCY_ALERT, buf);
}

void checkAlertThresholds() {
  String trigger = "";
  String message = "";
  String action  = "";

  if (current.dht_valid && current.temperature_c > ALERT_TEMP_HIGH_C) {
    trigger = "SENSOR_THRESHOLD";
    message = "Critical temp: " + String(current.temperature_c, 1) + "C";
    action  = "EMERGENCY_IRRIGATION";
  }
  else if (current.dht_valid && current.temperature_c < ALERT_TEMP_LOW_C) {
    trigger = "SENSOR_THRESHOLD";
    message = "Temp too low: " + String(current.temperature_c, 1) + "C";
    action  = "STOP_IRRIGATION";
  }
  else if (current.moisture_valid && current.soil_moisture_gm3 < ALERT_MOISTURE_LOW_GM3) {
    trigger = "SENSOR_THRESHOLD";
    message = "Moisture critical: " + String(current.soil_moisture_gm3, 0) + " g/m3";
    action  = "EMERGENCY_IRRIGATION";
  }
  else if (current.water_valid && current.water_level_pct < ALERT_WATER_LOW_PCT) {
    trigger = "SENSOR_THRESHOLD";
    message = "Tank low: " + String(current.water_level_pct, 1) + "%";
    action  = "REFILL_TANK";
  }
  else if (current.dht_valid && current.humidity_pct < ALERT_HUMIDITY_LOW_PCT) {
    trigger = "SENSOR_THRESHOLD";
    message = "Humidity low: " + String(current.humidity_pct, 1) + "%";
    action  = "EMERGENCY_IRRIGATION";
  }
  // FIX #2 extension: sensor fault triggers ERROR device state
  else if (!current.dht_valid) {
    if (!alertActive || lastAlertMessage != "DHT22_FAULT") {
      alertActive      = true;
      lastAlertMessage = "DHT22_FAULT";
      sendDeviceStateChange("ONLINE", "ERROR", "DHT22_SENSOR_FAULT");
    }
    return;
  }

  if (trigger.length() > 0) {
    if (!alertActive || lastAlertMessage != message) {
      alertActive      = true;
      lastAlertMessage = message;
      sendEmergencyAlert(trigger, message, action);
    }
  } else {
    if (alertActive) {
      alertActive      = false;
      lastAlertMessage = "";
      Serial.println("[ALERT] Condition cleared.");
    }
  }
}


//  VALVE CONTROL
void openValve(unsigned long durMs, const String& cmdId) {
  if (current.water_valid && current.water_level_pct < VALVE_MIN_WATER_PCT) {
    sendCommandAck(cmdId, "REJECTED", "CLOSED",
      "REJECTED: Tank " + String(current.water_level_pct, 1) + "% < 5% min");
    return;
  }
  if (valve.open) {
    valve.opened_at_ms    = millis();
    valve.duration_ms     = durMs;
    valve.timer_active    = (durMs > 0);
    valve.last_command_id = cmdId;
    sendCommandAck(cmdId, "EXECUTED", "OPEN", "Already open — timer reset.");
    return;
  }
  digitalWrite(PIN_VALVE, HIGH);
  valve.open            = true;
  valve.opened_at_ms    = millis();
  valve.duration_ms     = durMs;
  valve.timer_active    = (durMs > 0);
  valve.last_command_id = cmdId;
  valve.total_cycles++;
  valve.safety_triggered = false;

  String msg = "Valve opened.";
  if (durMs > 0) msg += " Timer: " + String(durMs / 60000UL) + " min.";
  else           msg += " No timer — manual close required.";
  sendCommandAck(cmdId, "EXECUTED", "OPEN", msg);
  Serial.printf("[VALVE] OPEN | dur=%lus | cmd=%s\n", durMs/1000UL, cmdId.c_str());
}

void closeValve(const String& cmdId, const String& reason) {
  if (!valve.open) {
    sendCommandAck(cmdId, "EXECUTED", "CLOSED", "Already closed.");
    return;
  }
  digitalWrite(PIN_VALVE, LOW);
  unsigned long wasOpen = (millis() - valve.opened_at_ms) / 1000UL;
  valve.open         = false;
  valve.timer_active = false;
  String msg = "Closed. Reason: " + reason + ". Was open " + String(wasOpen) + "s.";
  sendCommandAck(cmdId, "EXECUTED", "CLOSED", msg);
  Serial.printf("[VALVE] CLOSED | %s | was open %lus\n", reason.c_str(), wasOpen);
}

void tickValveTimer() {
  if (!valve.open) return;
  unsigned long elapsed = millis() - valve.opened_at_ms;

  if (valve.timer_active && elapsed >= valve.duration_ms) {
    closeValve(valve.last_command_id, "TIMER_EXPIRED");
    sendCommandAck(valve.last_command_id, "COMPLETED", "CLOSED",
      "Auto-closed after timer.");
    return;
  }

  if (elapsed >= (unsigned long)VALVE_WATCHDOG_MIN * 60000UL) {
    valve.safety_triggered = true;
    Serial.println("[WARN] Safety watchdog — force closing.");
    sendDeviceStateChange("ONLINE", "ERROR", "VALVE_WATCHDOG_65MIN");  // FIX #2
    closeValve("WATCHDOG", "SAFETY_WATCHDOG_65MIN");
    sendDeviceStateChange("ERROR", "ONLINE", "WATCHDOG_RESOLVED");     // FIX #2
  }
}


void executeValveCommand(const char* jsonStr) {
  StaticJsonDocument<384> doc;
  auto err = deserializeJson(doc, jsonStr);
  if (err) {
    sendCommandAck("UNKNOWN", "FAILED", valve.open ? "OPEN" : "CLOSED",
      "JSON error: " + String(err.c_str()));
    return;
  }

  if (String((const char*)doc["packet_type"]) != "VALVE_COMMAND") return;

  String cmdZone   = doc["zone_id"]   | "";
  String cmdDevice = doc["device_id"] | "";

  // Silent discard if not addressed to this node (spec §6.1)
  if (cmdZone != ZONE_ID || cmdDevice != DEVICE_ID) {
    Serial.printf("[RX] Ignored — zone=%s dev=%s not this node\n",
      cmdZone.c_str(), cmdDevice.c_str());
    return;
  }

  String cmdId = doc["command_id"]       | "UNKNOWN";
  String cmd   = doc["command"]          | "";
  int    durM  = doc["duration_minutes"] | 0;
  if (durM > VALVE_MAX_DURATION_MIN) durM = VALVE_MAX_DURATION_MIN;
  unsigned long durMs = (unsigned long)durM * 60000UL;

  Serial.printf("[CMD] %s | id=%s | dur=%dmin\n",
    cmd.c_str(), cmdId.c_str(), durM);

  if      (cmd == "OPEN_VALVE")  openValve(durMs, cmdId);
  else if (cmd == "CLOSE_VALVE") closeValve(cmdId, "REMOTE_COMMAND");
  else sendCommandAck(cmdId, "REJECTED", valve.open ? "OPEN" : "CLOSED",
    "Unknown command: " + cmd);
}


//  TRANSPORT LAYER — Serial placeholder (swap bodies for LoRa Week 4)


uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

void transmitPacket(uint8_t type, const char* payload) {
  uint16_t len = strlen(payload);
  uint16_t crc = crc16((const uint8_t*)payload, len);
  const char* names[] = {"","SENSOR_READING","COMMAND_ACK","DEVICE_STATE","EMERGENCY_ALERT"};
  Serial.printf("\n[TX:%s | %uB | crc:0x%04X]\n",
    type <= 4 ? names[type] : "UNKNOWN", len, crc);
  Serial.println(payload);

}

bool receivePacket(char* out, size_t maxLen) {
  if (!Serial.available()) return false;
  size_t n = Serial.readBytesUntil('\n', out, maxLen - 1);
  if (!n) return false;
  out[n] = '\0';
  return true;

}

void processSerial() {
  char rx[400];
  if (!receivePacket(rx, sizeof(rx))) return;
  String line = String(rx); line.trim();
  if (!line.length()) return;

  if (line.startsWith("{")) {
    executeValveCommand(rx);
  } else if (line == "status") {
    Serial.printf("[STATUS] M:%.0fg/m3 W:%.1f%% T:%.1fC H:%.1f%% "
                  "Valve:%s Cycles:%lu Alert:%s\n",
      current.soil_moisture_gm3, current.water_level_pct,
      current.temperature_c, current.humidity_pct,
      valve.open ? "OPEN" : "CLOSED", valve.total_cycles,
      alertActive ? lastAlertMessage.c_str() : "none");
  } else if (line == "ping") {
    sendDeviceStateChange("ONLINE", "ONLINE");
  } else if (line == "reset") {
    sendDeviceStateChange("ONLINE", "OFFLINE");
    delay(200);
    esp_restart();
  } else {
    Serial.printf("[RX] Unknown: '%s'\n", rx);
    Serial.println("[RX] Try: VALVE_COMMAND JSON | status | ping | reset");
  }
}

void updateLEDs() {
  float m = current.soil_moisture_gm3;
  digitalWrite(PIN_LED_DRY,     m < 600.0f                 ? HIGH : LOW);
  digitalWrite(PIN_LED_OPTIMAL, m >= 600.0f && m < 1400.0f ? HIGH : LOW);
  digitalWrite(PIN_LED_WET,     m >= 1400.0f               ? HIGH : LOW);
}

void tickHeartbeat() {
  digitalWrite(PIN_LED_STATUS, valve.open ? HIGH : (heartbeat ? HIGH : LOW));
}

void printBanner() {
  Serial.println();
  Serial.println("====================================================");
  Serial.println("  TAZROUT Node v" FW_VERSION " -- REAL HARDWARE");
  Serial.println("====================================================");
  Serial.printf( "  Device : %s | Zone: %s\n", DEVICE_ID, ZONE_ID);
  Serial.printf( "  ADC dry=%d wet=%d (calibrate with 'cal' cmd)\n",
    ADC_MOISTURE_DRY, ADC_MOISTURE_WET);
  Serial.println("  Architecture: ESP32 executes AI commands via LoRa");
  Serial.println("----------------------------------------------------");
  Serial.println("  Commands: VALVE_COMMAND JSON | status | ping | reset");
  Serial.println("====================================================");
  Serial.println();
}
