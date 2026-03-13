# TAZROUT Node v3.1 — Real Hardware Testing Guide
**Firmware:** `TAZROUT_Node_v2_REAL.ino`
**Developer:** KHENFRI Moussa | **Date:** March 2026

---

## Before You Start

Read this entire guide once before touching any wire. The testing sequence is strict —each phase must pass completely before moving to the next. Skipping ahead and debugging everything at once is how you waste three days on a problem that takes 10 minutes to isolate.

**Required hardware:**
- ESP32 DevKit v1 (ESP32-WROOM-32)
- DHT22 sensor + one 10kΩ resistor
- Capacitive soil moisture sensor v2.0 (corrosion-resistant)
- HC-SR04 ultrasonic sensor or analog water level sensor
- 5V single-channel relay module (optocoupler isolated)
- LEDs: 1× red, 1× yellow, 1× green + three 220Ω resistors
- Solderless breadboard + jumper wires
- USB cable (data, not charge-only)
- Multimeter

**Required software:**
- Arduino IDE 2.x
- ESP32 board package by Espressif (install via Board Manager)
- Libraries: `DHT sensor library` by Adafruit, `ArduinoJson` v6.21+

---

## Pin Reference (spec §3.2)

| Signal | GPIO | Notes |
|---|---|---|
| DHT22 data | 16 | 10kΩ pull-up between data pin and 3.3V |
| Soil moisture ADC | 35 | ADC1_CH7 — input only pin |
| Water level ADC | 34 | ADC1_CH6 — input only pin |
| Valve relay IN | 26 | Active HIGH — relay coil needs 5V supply |
| Heartbeat LED | 2 | Built-in LED on most DevKit boards |
| LED DRY (red) | 13 | 220Ω resistor in series to GND |
| LED OPTIMAL (yellow) | 12 | 220Ω resistor in series to GND |
| LED WET (green) | 14 | 220Ω resistor in series to GND |

**GPIO 34 and 35 are input-only.** They have no internal pull-up or pull-down. Do not try to use them as outputs.

---

## Phase 1 — Power and Serial

**Goal:** Confirm the ESP32 is alive and communicating.

**Wiring:** Nothing. Just the USB cable.

**Sketch to flash:**
```cpp
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 ALIVE");
}
void loop() {
  Serial.printf("uptime: %lu ms\n", millis());
  delay(1000);
}
```

**Pass criteria:**
- Arduino IDE uploads without error
- Serial Monitor (115200 baud) prints `ESP32 ALIVE` then uptime every second
- No garbled characters — if you see garbage, baud rate is wrong

**If it fails:**
- Check USB cable — many cheap cables are charge-only with no data lines
- Check COM port selected in Arduino IDE matches your device
- Hold BOOT button on ESP32 during upload if it gets stuck

---

## Phase 2 — GPIO Output (LEDs)

**Goal:** Confirm all four output pins work before connecting sensors.

**Wiring:**
```
GPIO 13 → 220Ω → LED red anode → LED cathode → GND
GPIO 12 → 220Ω → LED yellow anode → LED cathode → GND
GPIO 14 → 220Ω → LED green anode → LED cathode → GND
GPIO 2  → already connected to built-in LED
```

**Important:** LED anode (longer leg) connects toward the GPIO pin. Cathode (shorter leg) connects toward GND through the resistor.

**Sketch to flash:**
```cpp
void setup() {
  Serial.begin(115200);
  int pins[] = {2, 12, 13, 14};
  for (int p : pins) pinMode(p, OUTPUT);
}
void loop() {
  int pins[] = {2, 12, 13, 14};
  for (int p : pins) {
    digitalWrite(p, HIGH);
    Serial.printf("GPIO %d ON\n", p);
    delay(500);
    digitalWrite(p, LOW);
    delay(200);
  }
}
```

**Pass criteria:** Each LED blinks in sequence. Serial confirms which pin is HIGH at each moment.

**If a specific LED doesn't light:**
- Swap it with a known-working LED — the LED itself may be dead
- Check resistor connection with multimeter in continuity mode
- Verify GPIO number matches physical pin label on your DevKit

---

## Phase 3 — Relay

**Goal:** Confirm valve control GPIO works and relay clicks correctly.

**Wiring:**
```
Relay VCC → ESP32 5V (VIN pin, not 3.3V)
Relay GND → ESP32 GND
Relay IN  → GPIO 26
```

**Why 5V for the relay?** The relay coil requires 5V to energise reliably. The control signal (IN pin) accepts 3.3V logic from the ESP32 because the optocoupler handles the level shift. If you power the relay from 3.3V the coil may not pull in fully, leading to intermittent switching.

**Sketch to flash:**
```cpp
#define RELAY_PIN 26
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // ensure closed on boot
  Serial.println("Relay test — listen for clicks");
}
void loop() {
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("RELAY ON  (valve OPEN)");
  delay(2000);
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("RELAY OFF (valve CLOSED)");
  delay(2000);
}
```

**Pass criteria:**
- Audible click every 2 seconds
- Relay module LED turns on and off in sync
- Serial confirms state changes

**If no click:**
- Measure VCC pin on relay module with multimeter — must read ~5V
- Measure IN pin while HIGH — must read ~3.3V
- If voltages correct but no click, relay module may be defective

---

## Phase 4 — DHT22 Temperature and Humidity

**Goal:** Confirm temperature and humidity readings are valid.

**Wiring:**
```
DHT22 pin 1 (VCC) → 3.3V
DHT22 pin 2 (DATA) → GPIO 16
DHT22 pin 2 (DATA) → 10kΩ resistor → 3.3V  (pull-up)
DHT22 pin 4 (GND) → GND
```

The 10kΩ pull-up is not optional. DHT22 uses a single-wire protocol that requires the line to be pulled HIGH at rest. Without it you will get NaN on every read.

**Sketch to flash:**
```cpp
#include <DHT.h>
DHT dht(16, DHT22);

void setup() {
  Serial.begin(115200);
  dht.begin();
  delay(2000);
  Serial.println("DHT22 test");
}
void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println("ERROR: NaN — check wiring and pull-up resistor");
  } else {
    Serial.printf("Temp: %.1f C   Humidity: %.1f %%\n", t, h);
  }
  delay(2000);
}
```

**Pass criteria:**
- Temperature reads within ~1°C of room temperature (compare with phone thermometer)
- Humidity reads a plausible value (30–70% indoors is typical)
- No NaN errors

**If you get NaN every time:**
- Check the 10kΩ pull-up is connected between DATA and 3.3V (not 5V, not GND)
- Confirm GPIO 16 in sketch matches your physical wiring
- Try `delay(2500)` instead of `2000` — some DHT22 batches are slow to stabilise

**If values look wrong by a large margin:**
- DHT22 needs 30 seconds to equilibrate after power-on in a new environment
- Shield the sensor from direct airflow while taking reference readings

---

## Phase 5 — Soil Moisture Sensor (ADC Calibration)

**Goal:** Read raw ADC values and calibrate `ADC_MOISTURE_DRY` and `ADC_MOISTURE_WET` for your specific sensor.

This is the most important step. Every capacitive sensor batch is slightly different. The constants `2800` and `1200` in the firmware are estimates — your sensor will have different values. If you skip this calibration, moisture readings will be offset or compressed.

**Wiring:**
```
Sensor VCC → 3.3V
Sensor GND → GND
Sensor AOUT → GPIO 35
```

**Sketch to flash:**
```cpp
#include <driver/adc.h>

void setup() {
  Serial.begin(115200);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_12);
  Serial.println("Soil moisture calibration");
  Serial.println("Hold sensor in DRY AIR for 10 seconds...");
}
void loop() {
  int raw = adc1_get_raw(ADC1_CHANNEL_7);
  Serial.printf("RAW ADC: %d\n", raw);
  delay(500);
}
```

**Calibration procedure:**

**Step A — Dry reading:**
1. Hold the sensor in open air (not touching anything)
2. Wait 10 seconds for readings to stabilise
3. Note the raw ADC value — this is your `ADC_MOISTURE_DRY`
4. Expected range: 2600–3100

**Step B — Wet reading:**
1. Fill a cup with plain water
2. Submerge the sensor tip up to the line marked on the sensor (not the electronics)
3. Wait 10 seconds
4. Note the raw ADC value — this is your `ADC_MOISTURE_WET`
5. Expected range: 1000–1500

**Step C — Update firmware:**
Open `TAZROUT_Node_v3_fixed.ino` and update these two lines:
```cpp
#define ADC_MOISTURE_DRY   [your dry reading]
#define ADC_MOISTURE_WET   [your wet reading]
```

**Pass criteria:**
- Dry reading is consistently higher than wet reading (inverted — this is correct for capacitive sensors)
- The difference between dry and wet is at least 800 counts (less than this means poor contact or broken sensor)
- Values are stable (fluctuating by less than ±50 counts) once settled

**If dry and wet readings are similar (less than 500 counts apart):**
- Sensor may be resistive type, not capacitive — check the label on the board
- Check 3.3V supply — capacitive sensors are sensitive to supply voltage variation

---

## Phase 6 — Water Level Sensor (ADC Calibration)

**Goal:** Confirm GPIO 34 reads the water level sensor and calibrate the range.

**Wiring (analog voltage output type):**
```
Sensor VCC → 5V
Sensor GND → GND
Sensor OUT → GPIO 34
```

**Note:** If your sensor outputs 0–5V, you must use a voltage divider to bring it down to 0–3.3V before GPIO 34. GPIO 34 maximum input is 3.3V — exceeding this permanently damages the pin. A simple divider: 10kΩ from sensor OUT to GPIO 34, then 20kΩ from GPIO 34 to GND gives you ×0.67 scaling.

**Sketch to flash:**
```cpp
#include <driver/adc.h>

void setup() {
  Serial.begin(115200);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);
  Serial.println("Water level calibration");
}
void loop() {
  int raw = adc1_get_raw(ADC1_CHANNEL_6);
  Serial.printf("RAW ADC: %d\n", raw);
  delay(500);
}
```

**Calibration procedure:**

**Step A — Empty reading:**
1. Ensure the tank or container is empty
2. Note the raw ADC value — this is your `ADC_WATER_EMPTY`

**Step B — Full reading:**
1. Fill the tank to 100% capacity
2. Note the raw ADC value — this is your `ADC_WATER_FULL`

**Step C — Update firmware:**
```cpp
#define ADC_WATER_EMPTY   [your empty reading]
#define ADC_WATER_FULL    [your full reading]
```

**Pass criteria:**
- Reading changes consistently as water level changes
- Full reading is higher than empty (or lower — depends on your sensor type — adjust the `map()` call accordingly)

---

## Phase 7 — Full Firmware Flash

Only flash the production firmware after Phases 1–6 all pass with your calibrated values already written into the constants.

**Pre-flash checklist:**
- [ ] `ADC_MOISTURE_DRY` and `ADC_MOISTURE_WET` updated with your measured values
- [ ] `ADC_WATER_EMPTY` and `ADC_WATER_FULL` updated
- [ ] All five components wired and individually verified
- [ ] Relay is powered from 5V, not 3.3V
- [ ] DHT22 pull-up resistor in place
- [ ] GPIO 34 and 35 input voltage confirmed ≤ 3.3V

**Flash:** Open `TAZROUT_Node_v3_fixed.ino` in Arduino IDE → select your board and port → Upload.
---
## Phase 8 — Functional Testing

With the full firmware running, test each function using Serial Monitor at 115200 baud.

### Test 1 — Status command

Type `status` and press Enter.

**Expected:**
```
[STATUS] M:600g/m3 W:75.0% T:24.5C H:62.3% Valve:CLOSED Cycles:0 Alert:none
```

All values must be plausible. If any sensor shows `-999` or `SENSOR_FAULT` in the sensor reading packets, return to the relevant phase above.

---

### Test 2 — Open valve (timed)

Paste into Serial Monitor:
```json
{"packet_type":"VALVE_COMMAND","command_id":"CMD-TEST-01","issued_by":"AI_ENGINE","zone_id":"zone_a","device_id":"MCU-ZONE-A-001","command":"OPEN_VALVE","duration_minutes":1,"priority":"NORMAL","linked_decision_id":"DEC-001","reason":"hardware test"}
```

**Expected:**
```
[CMD] OPEN_VALVE | id=CMD-TEST-01 | dur=1min
[VALVE] OPEN | dur=60s | cmd=CMD-TEST-01
[TX:COMMAND_ACK] {"status":"EXECUTED","valve_state_after":"OPEN"}
```

- Relay clicks ON
- Heartbeat LED goes solid (not blinking)
- After 60 seconds: relay clicks OFF, two ACK packets (`EXECUTED` CLOSED + `COMPLETED`)

---

### Test 3 — Safety rejection

1. Disconnect or cover the water level sensor so it reads 0 (or set `ADC_WATER_EMPTY` temporarily to 0 and `ADC_WATER_FULL` to 10 to simulate low tank)
2. Send `OPEN_VALVE` command
3. Expected: `{"status":"REJECTED","message":"REJECTED: Tank X.X% < 5% min"}`
4. Relay must NOT click

---

### Test 4 — Emergency alert (temperature)

Temporarily change in firmware:
```cpp
#define ALERT_TEMP_HIGH_C   20.0f  // below room temp — will trigger immediately
```

Reflash, boot, wait for first sensor read cycle (30 seconds).

**Expected:**
```
[TX:EMERGENCY_ALERT] {"message":"Critical temp: 24.5C","recommended_action":"EMERGENCY_IRRIGATION"}
```

Wait another 30 seconds — **no second alert** (debounce working).

Restore `ALERT_TEMP_HIGH_C` to `45.0f` and reflash.

---

### Test 5 — Manual close

With valve open from Test 2, send:
```json
{"packet_type":"VALVE_COMMAND","command_id":"CMD-TEST-02","issued_by":"AI_ENGINE","zone_id":"zone_a","device_id":"MCU-ZONE-A-001","command":"CLOSE_VALVE","duration_minutes":0,"priority":"NORMAL","linked_decision_id":"DEC-002","reason":"manual test"}
```

**Expected:** Relay clicks OFF, `{"status":"EXECUTED","valve_state_after":"CLOSED"}`.

---

### Test 6 — Wrong zone (silent ignore)

Send with `zone_id` changed to `zone_b`:
```json
{"packet_type":"VALVE_COMMAND","command_id":"CMD-WRONG","issued_by":"AI_ENGINE","zone_id":"zone_b","device_id":"MCU-ZONE-A-001","command":"OPEN_VALVE","duration_minutes":1,"priority":"NORMAL","linked_decision_id":"DEC-003","reason":"wrong zone test"}
```

**Expected:** `[RX] Ignored — zone=zone_b dev=MCU-ZONE-A-001 not this node`

No relay click. No ACK packet.

---

### Test 7 — Ping

Type `ping` in Serial Monitor.

**Expected:** `DEVICE_STATE_CHANGE` packet with `ONLINE→ONLINE`.

---

## Phase 9 — 24-Hour Stability Run

Leave the node running for 24 hours with all sensors connected. Every 30 minutes check Serial Monitor briefly.

**What to watch for:**

| Problem | Symptom | Cause |
|---|---|---|
| DHT22 degrading | `SENSOR_FAULT` appearing in packets after hours | Loose connection or heat |
| ADC drift | Moisture value slowly drifting with unchanged conditions | Normal — sensors warm up. Note baseline after 1 hour |
| Memory leak | Uptime counter running but behavior getting erratic | Unlikely with this firmware — no dynamic allocation except String |
| Relay chatter | Clicking without commands | Electrical noise on GPIO 26 — add 100Ω series resistor on relay IN |

**Pass criteria after 24 hours:**
- Zero `SENSOR_FAULT` status in any `SENSOR_READING` packet
- DHT22 readings stable (±1°C, ±3% humidity)
- No unexpected valve activations
- Node still responding to `status` command

---

## Common Problems and Fixes

| Symptom | Most Likely Cause | Fix |
|---|---|---|
| Upload stuck at "Connecting..." | ESP32 not in flash mode | Hold BOOT button during upload start |
| NaN from DHT22 | Missing 10kΩ pull-up | Add resistor between DATA and 3.3V |
| Moisture always 0% or 100% | Wrong calibration constants | Redo Phase 5 with your specific sensor |
| Moisture inverted (wet = 0) | ADC_DRY and ADC_WET swapped | Swap the two constant values |
| Relay doesn't click | Powered from 3.3V instead of 5V | Move relay VCC to VIN/5V pin |
| Relay clicks on boot | GPIO 26 floating before pinMode | Already fixed — firmware sets LOW before other code runs |
| Water level stuck at 0% | Voltage divider missing (5V sensor) | Add voltage divider — see Phase 6 note |
| `SENSOR_FAULT` on moisture | ADC reading stuck at rail (0 or 4095) | Check sensor VCC is 3.3V, not 5V |
| Wrong zone silently ignored | You sent zone_b but device is zone_a | Use `zone_id: "zone_a"` in every command |
| Serial output garbled | Wrong baud rate in monitor | Set Serial Monitor to 115200 |

---

## After All Tests Pass

The node is readyfor  LoRa integration.

**Before wiring the SX1276 module**, you must reassign two pins that conflict:
- GPIO 2 (heartbeat LED) → LoRa needs this for DIO0 → move LED to GPIO 27
- GPIO 14 (WET LED) → LoRa needs this for RST → move LED to GPIO 25

Update `PIN_LED_STATUS` and `PIN_LED_WET` in the firmware before adding LoRa hardware. Do not attempt to run LoRa with the LED still on GPIO 2 — it will cause unpredictable radio behaviour.

---

*TAZROUT Smart Irrigation System — KHENFRI Moussa — March 2026*
