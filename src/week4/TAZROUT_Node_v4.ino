/*
 * TAZROUT Smart Irrigation System
 * ESP32 Field Sensor Node — Firmware v4.0.0
 *
 * Developer  : KHENFRI Moussa
 * Week       : 4 — LoRa Radio Integration
 * Spec ref   : TAZROUT ESP32 Technical Specification v1.0
 *
 * What this firmware does:
 *   - Reads soil moisture, water tank level, temperature, and humidity every 30 seconds
 *   - Sends compact JSON packets over LoRa radio to the Gateway
 *   - Listens for VALVE_COMMAND packets from the Gateway via LoRa
 *   - Controls the irrigation valve relay based on those commands
 *   - Sends alerts when sensors cross critical thresholds
 *

 * Packet size handling:
 *   LoRa maximum payload is 255 bytes.
 *   Our framing uses 4 header bytes and 2 CRC bytes, leaving 249 bytes for JSON.
 *   The original spec JSON format is 380-400 bytes — too large.
 *   This firmware uses compact key names. All packets measure under 200 bytes.
 *   The Python Gateway expands compact keys back to full names before MQTT publishing.
 *   Each send function documents its compact-to-full key mapping.
 *
 * SX1276 LoRa module wiring:
 *   VCC  -> 3.3V only, never 5V
 *   GND  -> GND
 *   SCK  -> GPIO 18
 *   MISO -> GPIO 19
 *   MOSI -> GPIO 23
 *   NSS  -> GPIO 5
 *   RST  -> GPIO 14
 *   DIO0 -> GPIO 2
 *
 * Required libraries (Arduino Library Manager):
 *   LoRa by Sandeep Mistry, version 0.8 or later
 *   DHT sensor library by Adafruit
 *   ArduinoJson, version 6.21 or later
 */

#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <driver/adc.h>
#include <esp_system.h>


// Node identity. Change DEVICE_ID when deploying a second node (MCU-ZONE-B-001, etc.)
#define DEVICE_ID   "MCU-ZONE-A-001"
#define ZONE_ID     "zone_a"
#define FW_VERSION  "4.0.0"

// Sensor GPIO pins
#define PIN_DHT          16
#define PIN_VALVE        26
#define PIN_LED_STATUS   27
#define PIN_LED_DRY      13
#define PIN_LED_OPTIMAL  12
#define PIN_LED_WET      25
#define DHT_TYPE         DHT22

// ADC channels for analog sensors
#define MOISTURE_CHANNEL  ADC1_CHANNEL_7   // GPIO 35
#define WATER_CHANNEL     ADC1_CHANNEL_6   // GPIO 34

// LoRa SX1276 pin mapping and radio parameters
#define LORA_PIN_CS    5
#define LORA_PIN_RST  14
#define LORA_PIN_DIO0  2
#define LORA_FREQUENCY 868E6
#define LORA_SF        7       // Spreading Factor 7: fastest, ~2km range
#define LORA_BW        125E3   // Bandwidth 125 kHz: standard
#define LORA_CR        5       // Coding Rate 4/5: standard error correction
#define LORA_POWER     17      // 17 dBm: good outdoor range without excess power

// Maximum JSON size per LoRa packet.
// LoRa allows 255 bytes total. Header = 4 bytes, CRC = 2 bytes, leaving 249.
// We cap at 220 as a safety margin.
#define MAX_PACKET_BYTES  220

// Protocol version byte included in every packet header
#define PROTOCOL_VERSION  0x01

// Sensor reading interval
#define SENSOR_READ_INTERVAL_MS  30000UL
#define HEARTBEAT_INTERVAL_MS      500UL
#define DHT_RETRY_DELAY_MS         500UL

#define LORA_RETRY_DELAY_MS  5000UL
#define LORA_MAX_RETRIES         3

// Moving average filter depth. 10 samples at 30s intervals = 5 minute flush time.
// This is intentional: soil moisture does not change meaningfully in seconds.
#define FILTER_SAMPLES  10

#define ADC_MOISTURE_DRY  2800   // sensor in open air
#define ADC_MOISTURE_WET  1200   // sensor submerged in water

// Water tank level ADC calibration
#define ADC_WATER_EMPTY   400
#define ADC_WATER_FULL   3600

// Emergency alert thresholds (spec section 5.4)
#define THRESHOLD_TEMP_HIGH   45.0f   // Celsius
#define THRESHOLD_TEMP_LOW     5.0f
#define THRESHOLD_MOISTURE   150.0f   // g/m3
#define THRESHOLD_WATER_LOW   10.0f   // percent
#define THRESHOLD_HUMIDITY    20.0f   // percent

// Valve safety limits (spec section 8.3)
#define VALVE_MAX_OPEN_MINUTES  60
#define VALVE_WATCHDOG_MINUTES  65
#define VALVE_MIN_WATER_PCT      5.0f

// Packet type bytes used in the LoRa frame header (spec section 7.2)
#define PKT_SENSOR_READING   0x01
#define PKT_COMMAND_ACK      0x02
#define PKT_DEVICE_STATE     0x03
#define PKT_EMERGENCY_ALERT  0x04
#define PKT_VALVE_COMMAND    0x05


struct SensorData {
    float temperature;
    float humidity;
    float soil_moisture_gm3;
    float water_level_pct;
    bool  dht_ok;
    bool  moisture_ok;
    bool  water_ok;
};

struct ValveData {
    bool          open;
    unsigned long opened_at_ms;
    unsigned long duration_ms;
    bool          timer_active;
    String        command_id;
    uint32_t      cycle_count;
};

struct Filter {
    int  samples[FILTER_SAMPLES];
    int  index;
    long total;
};

enum SensorStatus { STATUS_OK, STATUS_OUT_OF_RANGE, STATUS_FAULT };


DHT        dht(PIN_DHT, DHT_TYPE);
SensorData sensors;
ValveData  valve;
Filter     moisture_filter;
Filter     water_filter;

unsigned long last_sensor_read  = 0;
unsigned long last_heartbeat    = 0;
bool          heartbeat_state   = false;
uint32_t      alert_id_counter  = 0;

// Alert debounce state. Prevents the same alert from firing every read cycle.
// Resets when the condition clears, so a new breach fires a fresh alert.
bool   alert_active       = false;
String alert_last_message = "";

// LoRa diagnostics
uint32_t lora_tx_ok      = 0;
uint32_t lora_tx_failed  = 0;
uint32_t lora_rx_ok      = 0;
uint32_t lora_crc_errors = 0;
int      last_rssi        = 0;
float    last_snr         = 0.0f;


void     read_sensors();
int      filter_update(Filter& f, int value);
void     filter_warmup(Filter& f, adc1_channel_t channel);

void     send_sensor_reading();
void     send_command_ack(const String& cmd_id, const String& status,
                          const String& valve_state, const String& message);
void     send_device_state(const String& previous, const String& current,
                           const String& reason = "");
void     send_emergency_alert(const String& message, const String& action);
void     check_alert_thresholds();

void     open_valve(unsigned long duration_ms, const String& cmd_id);
void     close_valve(const String& cmd_id, const String& reason);
void     tick_valve_timer();
void     handle_valve_command(const char* json);

void     lora_transmit(uint8_t packet_type, const char* json);
bool     lora_receive(char* buffer, size_t buffer_size);
uint16_t crc16(const uint8_t* data, size_t length);

SensorStatus temp_status();
SensorStatus humidity_status();
SensorStatus moisture_status();
SensorStatus water_status();
const char*  status_string(SensorStatus s);

void     update_leds();
void     handle_serial_input();
void     print_boot_banner();


void setup() {
    Serial.begin(115200);
    delay(500);

    // Set all output pins to safe defaults before anything else.
    // The valve is always CLOSED at boot.
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

    // Configure ADC at driver level.
    // ADC_ATTEN_DB_12 enables the full 0-3.3V input range.
    // The default Arduino analogRead() clips at about 1V, which makes
    // capacitive moisture sensor readings (1.2-2.8V range) mostly useless.
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(MOISTURE_CHANNEL, ADC_ATTEN_DB_12);
    adc1_config_channel_atten(WATER_CHANNEL,    ADC_ATTEN_DB_12);

    dht.begin();
    filter_warmup(moisture_filter, MOISTURE_CHANNEL);
    filter_warmup(water_filter,    WATER_CHANNEL);
    memset(&valve, 0, sizeof(valve));
    delay(2000);  // DHT22 stabilization

    // Start the LoRa radio. Halt if initialization fails.
    LoRa.setPins(LORA_PIN_CS, LORA_PIN_RST, LORA_PIN_DIO0);
    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("LoRa initialization failed.");
        Serial.println("Check wiring: CS=GPIO5, RST=GPIO14, DIO0=GPIO2.");
        Serial.println("Also confirm the module is 868 MHz, not 915 MHz.");
        while (true) delay(1000);
    }

    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setTxPower(LORA_POWER);

    print_boot_banner();
    send_device_state("OFFLINE", "ONLINE");
}


void loop() {
    unsigned long now = millis();

    if (now - last_sensor_read >= SENSOR_READ_INTERVAL_MS) {
        last_sensor_read = now;
        read_sensors();
        update_leds();
        send_sensor_reading();
        check_alert_thresholds();
    }

    tick_valve_timer();

    // Check for incoming LoRa packets every loop iteration
    char rx_buffer[MAX_PACKET_BYTES + 1];
    if (lora_receive(rx_buffer, sizeof(rx_buffer))) {
        handle_valve_command(rx_buffer);
    }

    // Serial monitor commands work when a PC is connected — useful for debugging
    handle_serial_input();

    if (now - last_heartbeat >= HEARTBEAT_INTERVAL_MS) {
        last_heartbeat  = now;
        heartbeat_state = !heartbeat_state;
        // Solid LED when valve is open, blinking when closed
        digitalWrite(PIN_LED_STATUS,
            valve.open ? HIGH : (heartbeat_state ? HIGH : LOW));
    }
}


void read_sensors() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
        delay(DHT_RETRY_DELAY_MS);
        t = dht.readTemperature();
        h = dht.readHumidity();
    }
    sensors.dht_ok      = !isnan(t) && !isnan(h);
    sensors.temperature = sensors.dht_ok ? t : -999.0f;
    sensors.humidity    = sensors.dht_ok ? h : -999.0f;

    // Capacitive moisture sensor: lower ADC = wetter soil (inverted output)
    int raw_m  = adc1_get_raw(MOISTURE_CHANNEL);
    int filt_m = filter_update(moisture_filter, raw_m);
    int pct_m  = constrain(map(filt_m, ADC_MOISTURE_DRY, ADC_MOISTURE_WET, 0, 100), 0, 100);
    sensors.soil_moisture_gm3 = pct_m * 20.0f;  // convert 0-100% to 0-2000 g/m3
    sensors.moisture_ok       = (filt_m > 50 && filt_m < 4050);  // detect stuck ADC

    int raw_w  = adc1_get_raw(WATER_CHANNEL);
    int filt_w = filter_update(water_filter, raw_w);
    int pct_w  = constrain(map(filt_w, ADC_WATER_EMPTY, ADC_WATER_FULL, 0, 100), 0, 100);
    sensors.water_level_pct = (float)pct_w;
    sensors.water_ok        = (filt_w > 50 && filt_w < 4050);
}


int filter_update(Filter& f, int value) {
    f.total        -= f.samples[f.index];
    f.samples[f.index] = value;
    f.total        += value;
    f.index         = (f.index + 1) % FILTER_SAMPLES;
    return (int)(f.total / FILTER_SAMPLES);
}

void filter_warmup(Filter& f, adc1_channel_t channel) {
    memset(&f, 0, sizeof(f));
    for (int i = 0; i < FILTER_SAMPLES; i++) {
        f.samples[i] = adc1_get_raw(channel);
        f.total     += f.samples[i];
        delay(5);
    }
}


SensorStatus temp_status() {
    if (!sensors.dht_ok) return STATUS_FAULT;
    if (sensors.temperature < -40 || sensors.temperature > 80) return STATUS_OUT_OF_RANGE;
    return STATUS_OK;
}
SensorStatus humidity_status() {
    if (!sensors.dht_ok) return STATUS_FAULT;
    if (sensors.humidity < 0 || sensors.humidity > 100) return STATUS_OUT_OF_RANGE;
    return STATUS_OK;
}
SensorStatus moisture_status() { return sensors.moisture_ok ? STATUS_OK : STATUS_FAULT; }
SensorStatus water_status()    { return sensors.water_ok    ? STATUS_OK : STATUS_FAULT; }

const char* status_string(SensorStatus s) {
    switch (s) {
        case STATUS_OK:           return "OK";
        case STATUS_OUT_OF_RANGE: return "OOR";
        case STATUS_FAULT:        return "FAULT";
        default:                  return "UNKNOWN";
    }
}


// SENSOR_READING — sent every 30 seconds
// Compact key mapping (expanded by Gateway before MQTT publish):
//   pkt -> packet_type   "SR" = SENSOR_READING
//   z   -> zone_id
//   d   -> device_id
//   t   -> temperature value (Celsius)
//   m   -> soil_moisture value (g/m3)
//   w   -> water_level value (%)
//   h   -> humidity value (%)
//   ts  -> temperature status
//   ms  -> soil_moisture status
//   ws  -> water_level status
//   hs  -> humidity status
// rssi and snr are filled in by the Gateway after reception.
// Measured size: ~135 bytes
void send_sensor_reading() {
    StaticJsonDocument<280> doc;
    doc["pkt"] = "SR";
    doc["z"]   = ZONE_ID;
    doc["d"]   = DEVICE_ID;
    doc["t"]   = serialized(String(sensors.temperature, 1));
    doc["m"]   = serialized(String(sensors.soil_moisture_gm3, 1));
    doc["w"]   = serialized(String(sensors.water_level_pct, 1));
    doc["h"]   = serialized(String(sensors.humidity, 1));
    doc["ts"]  = status_string(temp_status());
    doc["ms"]  = status_string(moisture_status());
    doc["ws"]  = status_string(water_status());
    doc["hs"]  = status_string(humidity_status());

    char buf[MAX_PACKET_BYTES + 1];
    serializeJson(doc, buf, sizeof(buf));
    lora_transmit(PKT_SENSOR_READING, buf);
}

// COMMAND_ACK — sent immediately after any valve command is received
// Compact key mapping:
//   pkt -> packet_type   "ACK"
//   z   -> zone_id
//   d   -> device_id
//   cid -> command_id
//   s   -> status  (EXECUTED, REJECTED, FAILED, COMPLETED)
//   v   -> valve_state_after
//   msg -> message
// Measured size: ~130 bytes
void send_command_ack(const String& cmd_id, const String& status,
                      const String& valve_state, const String& message) {
    StaticJsonDocument<230> doc;
    doc["pkt"] = "ACK";
    doc["z"]   = ZONE_ID;
    doc["d"]   = DEVICE_ID;
    doc["cid"] = cmd_id;
    doc["s"]   = status;
    doc["v"]   = valve_state;
    doc["msg"] = message;

    char buf[MAX_PACKET_BYTES + 1];
    serializeJson(doc, buf, sizeof(buf));
    lora_transmit(PKT_COMMAND_ACK, buf);
}

// DEVICE_STATE_CHANGE — sent on boot, safety watchdog, and sensor fault
// Compact key mapping:
//   pkt -> packet_type   "STATE"
//   z   -> zone_id
//   d   -> device_id
//   p   -> previous_device_state
//   c   -> current_device_state
//   v   -> valve_state
//   fw  -> firmware_version
//   bat -> battery_level  (-1 means USB powered)
//   why -> reason (optional)
// Measured size: ~110 bytes
void send_device_state(const String& previous, const String& current,
                       const String& reason) {
    StaticJsonDocument<200> doc;
    doc["pkt"] = "STATE";
    doc["z"]   = ZONE_ID;
    doc["d"]   = DEVICE_ID;
    doc["p"]   = previous;
    doc["c"]   = current;
    doc["v"]   = valve.open ? "OPEN" : "CLOSED";
    doc["fw"]  = FW_VERSION;
    doc["bat"] = -1;
    if (reason.length() > 0) doc["why"] = reason;

    char buf[MAX_PACKET_BYTES + 1];
    serializeJson(doc, buf, sizeof(buf));
    lora_transmit(PKT_DEVICE_STATE, buf);
}

// EMERGENCY_ALERT — sent once when a threshold is first crossed
// Compact key mapping:
//   pkt -> packet_type   "ALERT"
//   z   -> zone_id
//   d   -> device_id
//   id  -> alert_id
//   t   -> sensor_values.temperature
//   m   -> sensor_values.soil_moisture
//   w   -> sensor_values.water_level
//   h   -> sensor_values.humidity
//   msg -> message
//   act -> recommended_action
// Measured size: ~175 bytes
void send_emergency_alert(const String& message, const String& action) {
    StaticJsonDocument<280> doc;
    char alert_id[24];
    snprintf(alert_id, sizeof(alert_id), "ALERT-%05lu", ++alert_id_counter);

    doc["pkt"] = "ALERT";
    doc["z"]   = ZONE_ID;
    doc["d"]   = DEVICE_ID;
    doc["id"]  = alert_id;
    doc["t"]   = sensors.dht_ok ? sensors.temperature     : -999.0f;
    doc["m"]   = sensors.soil_moisture_gm3;
    doc["w"]   = sensors.water_level_pct;
    doc["h"]   = sensors.dht_ok ? sensors.humidity        : -999.0f;
    doc["msg"] = message;
    doc["act"] = action;

    char buf[MAX_PACKET_BYTES + 1];
    serializeJson(doc, buf, sizeof(buf));
    lora_transmit(PKT_EMERGENCY_ALERT, buf);
}


void check_alert_thresholds() {
    String message = "";
    String action  = "";

    if (sensors.dht_ok && sensors.temperature > THRESHOLD_TEMP_HIGH) {
        message = "Temp critical: " + String(sensors.temperature, 1) + "C";
        action  = "EMERGENCY_IRRIGATION";
    } else if (sensors.dht_ok && sensors.temperature < THRESHOLD_TEMP_LOW) {
        message = "Temp too low: " + String(sensors.temperature, 1) + "C";
        action  = "STOP_IRRIGATION";
    } else if (sensors.moisture_ok && sensors.soil_moisture_gm3 < THRESHOLD_MOISTURE) {
        message = "Moisture critical: " + String(sensors.soil_moisture_gm3, 0) + "g/m3";
        action  = "EMERGENCY_IRRIGATION";
    } else if (sensors.water_ok && sensors.water_level_pct < THRESHOLD_WATER_LOW) {
        message = "Tank critically low: " + String(sensors.water_level_pct, 1) + "%";
        action  = "REFILL_TANK";
    } else if (sensors.dht_ok && sensors.humidity < THRESHOLD_HUMIDITY) {
        message = "Humidity critical: " + String(sensors.humidity, 1) + "%";
        action  = "EMERGENCY_IRRIGATION";
    } else if (!sensors.dht_ok) {
        if (!alert_active || alert_last_message != "DHT22_FAULT") {
            alert_active       = true;
            alert_last_message = "DHT22_FAULT";
            send_device_state("ONLINE", "ERROR", "DHT22_FAULT");
        }
        return;
    }

    if (message.length() > 0) {
        // Only send if this is a new condition or a different condition than last time
        if (!alert_active || alert_last_message != message) {
            alert_active       = true;
            alert_last_message = message;
            send_emergency_alert(message, action);
        }
    } else {
        if (alert_active) {
            alert_active       = false;
            alert_last_message = "";
            Serial.println("Alert condition cleared.");
        }
    }
}


void open_valve(unsigned long duration_ms, const String& cmd_id) {
    if (sensors.water_ok && sensors.water_level_pct < VALVE_MIN_WATER_PCT) {
        send_command_ack(cmd_id, "REJECTED", "CLOSED",
            "Tank " + String(sensors.water_level_pct, 1) + "% is below the 5% minimum.");
        return;
    }

    if (valve.open) {
        valve.opened_at_ms = millis();
        valve.duration_ms  = duration_ms;
        valve.timer_active = (duration_ms > 0);
        valve.command_id   = cmd_id;
        send_command_ack(cmd_id, "EXECUTED", "OPEN", "Already open. Timer reset.");
        return;
    }

    digitalWrite(PIN_VALVE, HIGH);
    valve.open         = true;
    valve.opened_at_ms = millis();
    valve.duration_ms  = duration_ms;
    valve.timer_active = (duration_ms > 0);
    valve.command_id   = cmd_id;
    valve.cycle_count++;

    String msg = "Valve opened.";
    if (duration_ms > 0) {
        msg += " Auto-close in " + String(duration_ms / 60000UL) + " min.";
    } else {
        msg += " No timer. Manual close required.";
    }
    send_command_ack(cmd_id, "EXECUTED", "OPEN", msg);
    Serial.printf("Valve OPEN — %lus — command %s\n", duration_ms / 1000UL, cmd_id.c_str());
}

void close_valve(const String& cmd_id, const String& reason) {
    if (!valve.open) {
        send_command_ack(cmd_id, "EXECUTED", "CLOSED", "Valve was already closed.");
        return;
    }
    digitalWrite(PIN_VALVE, LOW);
    unsigned long was_open_s = (millis() - valve.opened_at_ms) / 1000UL;
    valve.open         = false;
    valve.timer_active = false;
    send_command_ack(cmd_id, "EXECUTED", "CLOSED",
        "Closed after " + String(was_open_s) + "s. Reason: " + reason);
    Serial.printf("Valve CLOSED — %s — was open %lus\n", reason.c_str(), was_open_s);
}

void tick_valve_timer() {
    if (!valve.open) return;
    unsigned long elapsed = millis() - valve.opened_at_ms;

    if (valve.timer_active && elapsed >= valve.duration_ms) {
        close_valve(valve.command_id, "TIMER_EXPIRED");
        send_command_ack(valve.command_id, "COMPLETED", "CLOSED",
            "Valve auto-closed after timer expired.");
        return;
    }

    // Force-close after 65 minutes regardless of any timer.
    // This is the last-resort safety net against any bug or lost close command.
    if (elapsed >= (unsigned long)VALVE_WATCHDOG_MINUTES * 60000UL) {
        Serial.println("Safety watchdog: force-closing valve at 65 minutes.");
        send_device_state("ONLINE", "ERROR", "VALVE_WATCHDOG");
        close_valve("WATCHDOG", "SAFETY_WATCHDOG_65MIN");
        send_device_state("ERROR", "ONLINE", "WATCHDOG_RESOLVED");
    }
}


// Parse an incoming VALVE_COMMAND packet and execute it.
// Expected compact format (sent by the Python Gateway):
//   pkt     "CMD"
//   z       zone_id — must match ZONE_ID, otherwise silently discard
//   d       device_id — must match DEVICE_ID, otherwise silently discard
//   cid     command_id
//   command "OPEN_VALVE" or "CLOSE_VALVE"
//   dur     duration in minutes (0 = no auto-close timer)
void handle_valve_command(const char* json) {
    StaticJsonDocument<200> doc;
    auto err = deserializeJson(doc, json);
    if (err) {
        send_command_ack("UNKNOWN", "FAILED", valve.open ? "OPEN" : "CLOSED",
            "JSON parse error: " + String(err.c_str()));
        return;
    }

    if (String((const char*)doc["pkt"]) != "CMD") return;
.
    if (String((const char*)doc["z"]) != ZONE_ID)   return;
    if (String((const char*)doc["d"]) != DEVICE_ID) return;

    String cmd_id  = doc["cid"]     | "UNKNOWN";
    String command = doc["command"] | "";
    int    dur_min = doc["dur"]     | 0;

    if (dur_min > VALVE_MAX_OPEN_MINUTES) dur_min = VALVE_MAX_OPEN_MINUTES;
    unsigned long dur_ms = (unsigned long)dur_min * 60000UL;

    Serial.printf("Command: %s — id: %s — duration: %dmin — RSSI: %ddBm — SNR: %.1fdB\n",
        command.c_str(), cmd_id.c_str(), dur_min, last_rssi, last_snr);

    if      (command == "OPEN_VALVE")  open_valve(dur_ms, cmd_id);
    else if (command == "CLOSE_VALVE") close_valve(cmd_id, "REMOTE_COMMAND");
    else send_command_ack(cmd_id, "REJECTED", valve.open ? "OPEN" : "CLOSED",
            "Unknown command value: " + command);
}


// CRC-16/XMODEM: polynomial 0x1021, initial value 0x0000.
// Both this firmware and the Python Gateway must use the same implementation.
uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

// Send a framed LoRa packet.
// Frame structure (spec section 7.2):
//   Bytes 0-1   JSON length as big-endian uint16
//   Byte  2     Packet type identifier
//   Byte  3     Protocol version (0x01)
//   Bytes 4-N   JSON payload
//   Bytes N+1-2 CRC-16/XMODEM over the JSON payload only
void lora_transmit(uint8_t packet_type, const char* json) {
    uint16_t json_len = (uint16_t)strlen(json);

    if (json_len > MAX_PACKET_BYTES) {
        Serial.printf("Warning: JSON is %u bytes, exceeds %d byte limit. Truncating.\n",
            json_len, MAX_PACKET_BYTES);
        json_len = MAX_PACKET_BYTES;
    }

    uint16_t checksum = crc16((const uint8_t*)json, json_len);

    for (int attempt = 1; attempt <= LORA_MAX_RETRIES; attempt++) {
        LoRa.beginPacket();
        LoRa.write((uint8_t)(json_len >> 8));
        LoRa.write((uint8_t)(json_len & 0xFF));
        LoRa.write(packet_type);
        LoRa.write(PROTOCOL_VERSION);
        LoRa.write((const uint8_t*)json, json_len);
        LoRa.write((uint8_t)(checksum >> 8));
        LoRa.write((uint8_t)(checksum & 0xFF));

        if (LoRa.endPacket()) {
            lora_tx_ok++;
            Serial.printf("TX — type 0x%02X — %u bytes — crc 0x%04X\n",
                packet_type, json_len, checksum);
            return;
        }

        Serial.printf("TX failed (attempt %d/%d). Retrying in %lus.\n",
            attempt, LORA_MAX_RETRIES, LORA_RETRY_DELAY_MS / 1000UL);
        delay(LORA_RETRY_DELAY_MS);
    }

    lora_tx_failed++;
    Serial.printf("TX gave up after %d attempts. Total TX failures: %lu.\n",
        LORA_MAX_RETRIES, lora_tx_failed);
}

// Poll for an incoming LoRa packet.
// Returns true and writes the JSON payload to buffer if a valid packet arrived.
// Returns false if no packet or CRC mismatch.
bool lora_receive(char* buffer, size_t buffer_size) {
    int packet_size = LoRa.parsePacket();
    if (packet_size < 7) return false;  // need at least 4 header + 1 JSON + 2 CRC

    // Read and discard the 4-byte header
    LoRa.read();  // length high
    LoRa.read();  // length low
    LoRa.read();  // packet type (validated by handle_valve_command via the pkt field)
    LoRa.read();  // protocol version

    // Read JSON payload, leaving 2 bytes for CRC
    int json_len = 0;
    while (LoRa.available() > 2) {
        if (json_len < (int)buffer_size - 1) {
            buffer[json_len++] = (char)LoRa.read();
        } else {
            LoRa.read();  // discard if buffer is full
        }
    }
    buffer[json_len] = '\0';

    uint8_t  crc_hi   = LoRa.read();
    uint8_t  crc_lo   = LoRa.read();
    uint16_t rx_crc   = ((uint16_t)crc_hi << 8) | crc_lo;
    uint16_t calc_crc = crc16((const uint8_t*)buffer, json_len);

    if (rx_crc != calc_crc) {
        lora_crc_errors++;
        Serial.printf("RX CRC mismatch — received 0x%04X, calculated 0x%04X. Discarded.\n",
            rx_crc, calc_crc);
        return false;
    }

    last_rssi = LoRa.packetRssi();
    last_snr  = LoRa.packetSnr();
    lora_rx_ok++;
    Serial.printf("RX OK — %d bytes — RSSI %d dBm — SNR %.1f dB\n",
        json_len, last_rssi, last_snr);
    return true;
}


void update_leds() {
    float m = sensors.soil_moisture_gm3;
    digitalWrite(PIN_LED_DRY,     m < 600.0f                  ? HIGH : LOW);
    digitalWrite(PIN_LED_OPTIMAL, m >= 600.0f && m < 1400.0f  ? HIGH : LOW);
    digitalWrite(PIN_LED_WET,     m >= 1400.0f                 ? HIGH : LOW);
}

void handle_serial_input() {
    if (!Serial.available()) return;
    char line[250];
    size_t n = Serial.readBytesUntil('\n', line, sizeof(line) - 1);
    if (!n) return;
    line[n] = '\0';
    String cmd = String(line);
    cmd.trim();

    if (cmd.startsWith("{")) {
        handle_valve_command(line);
    } else if (cmd == "status") {
        Serial.printf("Soil %.0f g/m3 | Water %.1f%% | Temp %.1f C | Humid %.1f%% | Valve %s | Cycles %lu\n",
            sensors.soil_moisture_gm3, sensors.water_level_pct,
            sensors.temperature, sensors.humidity,
            valve.open ? "OPEN" : "CLOSED", valve.cycle_count);
    } else if (cmd == "lora") {
        Serial.printf("TX ok %lu | TX failed %lu | RX ok %lu | CRC errors %lu | RSSI %d dBm | SNR %.1f dB\n",
            lora_tx_ok, lora_tx_failed, lora_rx_ok, lora_crc_errors, last_rssi, last_snr);
    } else if (cmd == "ping") {
        send_device_state("ONLINE", "ONLINE");
    } else if (cmd == "reset") {
        send_device_state("ONLINE", "OFFLINE");
        delay(200);
        esp_restart();
    } else if (cmd.length() > 0) {
        Serial.println("Commands: status | lora | ping | reset | VALVE_COMMAND JSON");
    }
}

void print_boot_banner() {
    Serial.println();
    Serial.println("TAZROUT Sensor Node v" FW_VERSION);
    Serial.printf( "Device: %s | Zone: %s\n", DEVICE_ID, ZONE_ID);
    Serial.printf( "LoRa: 868 MHz | SF%d | BW %.0f kHz | CR 4/%d | %d dBm\n",
        LORA_SF, LORA_BW / 1000.0, LORA_CR, LORA_POWER);
    Serial.printf( "ADC calibration: dry=%d wet=%d\n", ADC_MOISTURE_DRY, ADC_MOISTURE_WET);
    Serial.println("Serial commands: status | lora | ping | reset");
    Serial.println();
}
