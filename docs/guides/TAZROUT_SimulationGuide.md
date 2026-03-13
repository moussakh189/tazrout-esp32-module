# TAZROUT Node v3 — Simulation Guide & Hardware Testing Strategy
**Developer:** KHENFRI Moussa | **Firmware:** v3.0.0-SIM | **Updated:** March 2026

---

## Part 1 — Wokwi Simulation

### What the simulation is

`TAZROUT_Node_v2_SIM.ino` + `diagram_v3.json`  in week 3 codes  is a Wokwi project that runs the exact same application logic as the production firmware. The only differences are in the transport layer (Serial instead of LoRa) and sensor mapping (direct potentiometer instead of inverted capacitive sensor). Every JSON packet, every valve safety check, every ACK — identical to real hardware.

### How to open it

1. Go to [wokwi.com](https://wokwi.com) → **New Project** → **ESP32**
2. Replace the default `sketch.ino` content with `TAZROUT_Node_v3_SIM.ino`
3. Replace the default `diagram.json` content with `diagram_v3.json`
4. Click **Start Simulation**

### What you see at boot

```
╔══════════════════════════════════════════════════════════╗
║   TAZROUT Node v3.0.0-SIM  — WOKWI SIMULATION          ║
╠══════════════════════════════════════════════════════════╣
║  POT A (GPIO35) → Soil Moisture | right=wet, left=dry   ║
║  POT B (GPIO34) → Water Level   | right=full, left=low  ║
║  DHT22 (GPIO4)  → Temp/Humidity | adjust in component   ║
╠══════════════════════════════════════════════════════════╣
║  DEMO: Paste in Serial to test valve: ...               ║
╚══════════════════════════════════════════════════════════╝

[TX:DEVICE_STATE | 142B | crc:0x...]
{"packet_type":"DEVICE_STATE_CHANGE","zone_id":"zone_a","device_id":"MCU-ZONE-A-001",
 "previous_device_state":"OFFLINE","current_device_state":"ONLINE","valve_state":"CLOSED",
 "firmware_version":"3.0.0-SIM","battery_level":-1}
```

---

## Part 2 — The Components

### Potentiometer A — GPIO 35 — Soil Moisture

| Position | Moisture Reading | LED State | Meaning |
|---|---|---|---|
| Full left (0%) | 0 g/m³ | 🔴 Red ON | Critically dry |
| ~30% left | ~600 g/m³ | 🔴 Red ON | Dry threshold |
| Middle | ~1000 g/m³ | 🟡 Yellow ON | Optimal soil |
| ~70% right | ~1400 g/m³ | 🟡 Yellow ON | Upper optimal |
| Full right (100%) | 2000 g/m³ | 🟢 Green ON | Saturated |

**Every 5 seconds** the firmware reads this and emits a `SENSOR_READING` packet:

```json
{
  "packet_type": "SENSOR_READING",
  "zone_id": "zone_a",
  "device_id": "MCU-ZONE-A-001",
  "sensors": {
    "soil_moisture": { "value": "540.0", "unit": "g/m3", "status": "OK" },
    "temperature":   { "value": "24.0",  "unit": "C",    "status": "OK" },
    "water_level":   { "value": "80.0",  "unit": "%",    "status": "OK" },
    "humidity":      { "value": "60.0",  "unit": "%",    "status": "OK" }
  }
}
```

### Potentiometer B — GPIO 34 — Water Level

Controls the simulated water tank level (0–100%). This directly affects valve safety.

| Position | Water Level | Effect on OPEN_VALVE |
|---|---|---|
| Full left | 0% |  REJECTED — below 5% minimum |
| ~10% right | ~10% |  REJECTED — still below minimum |
| ~15% right | ~15% |  ACCEPTED — above 5% threshold |
| Full right | 100% |  ACCEPTED — tank full |

### DHT22 — GPIO 4 — Temperature & Humidity

Click the DHT22 component in Wokwi to change values. This is how you trigger emergency alerts.

| Setting | Effect |
|---|---|
| Temperature = 24°C, Humidity = 60% | Normal operation |
| Temperature = 46°C | `EMERGENCY_ALERT` fires → `recommended_action: EMERGENCY_IRRIGATION` |
| Temperature = 4°C | `EMERGENCY_ALERT` fires → `recommended_action: STOP_IRRIGATION` |
| Humidity = 19% | `EMERGENCY_ALERT` fires |

### Blue LED — GPIO 26 — Valve State

| LED State | Meaning |
|---|---|
| OFF | Valve CLOSED — normal/idle |
| ON (solid) | Valve OPEN — irrigation active |

---

## Part 3 — Demo Scenarios (Step by Step)

### Scenario 1 — Normal sensor reading cycle

1. Start simulation
2. Watch Serial Monitor — every 5 seconds a `SENSOR_READING` packet prints
3. Turn Pot A slowly — watch moisture value change in real time
4. Turn Pot A to far left → Red LED turns on (dry)
5. Turn Pot A to middle → Yellow LED (optimal)
6. Turn Pot A to far right → Green LED (wet)

**What this demonstrates:** Full sensor acquisition, ADC reading, moving average filter, LED status indicators, JSON packet generation.

---

### Scenario 2 — Receiving and executing a VALVE_COMMAND

This is the core demo. The ESP32 receives a command from the "AI" and executes it.

**Step 1:** Make sure Pot B is at ~50% (water level ~50% — above the 5% minimum).

**Step 2:** Paste this into the Wokwi Serial Monitor and press Enter:

```json
{"packet_type":"VALVE_COMMAND","command_id":"CMD-2026-0112","issued_by":"AI_ENGINE","zone_id":"zone_a","device_id":"MCU-ZONE-A-001","command":"OPEN_VALVE","duration_minutes":1,"priority":"NORMAL","linked_decision_id":"DEC-001","reason":"AI decision - moisture low"}
```

**Step 3:** Watch the Serial Monitor output:

```
[RX] Received: OPEN_VALVE | cmdId: CMD-2026-0112 | duration: 1min
[VALVE] OPEN | Duration: 60s | Cmd: CMD-2026-0112

[TX:COMMAND_ACK | 198B | crc:0xABCD]
{"packet_type":"COMMAND_ACK","command_id":"CMD-2026-0112","zone_id":"zone_a",
 "device_id":"MCU-ZONE-A-001","status":"EXECUTED","valve_state_after":"OPEN",
 "message":"Valve opened successfully. Timer: 1 min."}
```

**Step 4:** Blue LED turns ON. Wait 1 minute.

**Step 5:** Timer expires — auto-close:

```
[VALVE] Timer expired — auto-closing.
[VALVE] CLOSED | Reason: TIMER_EXPIRED | Was open 60s

[TX:COMMAND_ACK | 201B | crc:0x...]
{"packet_type":"COMMAND_ACK","command_id":"CMD-2026-0112","status":"COMPLETED",
 "valve_state_after":"CLOSED","message":"Valve auto-closed after timer expiry."}
```

**Blue LED turns OFF.** Full command lifecycle demonstrated.

---

### Scenario 3 — Safety rejection (low water level)

**Step 1:** Turn Pot B to far left (water level ~0%).

**Step 2:** Paste the same `OPEN_VALVE` command.

**Expected output:**

```
[TX:COMMAND_ACK | ...]
{"packet_type":"COMMAND_ACK","command_id":"CMD-2026-0112","status":"REJECTED",
 "valve_state_after":"CLOSED",
 "message":"REJECTED: Water level 2.0% below minimum 5%"}
```

Blue LED stays OFF. The valve was never opened. This is spec §8.3 working correctly.

---

### Scenario 4 — Emergency alert

**Step 1:** Click the DHT22 component in Wokwi.

**Step 2:** Set temperature to `46` (above 45°C threshold).

**Step 3:** Wait for the next 5-second read cycle.

**Expected output:**

```
[TX:EMERGENCY_ALERT | ...]
{"packet_type":"EMERGENCY_ALERT","alert_id":"ALERT-00001","severity":"CRITICAL",
 "issued_by":"MCU-ZONE-A-001","zone_id":"zone_a","triggered_by":"SENSOR_THRESHOLD",
 "sensor_values":{"temperature":46.0,"soil_moisture":...,"water_level":...},
 "message":"Critical temperature spike: 46.0C",
 "recommended_action":"EMERGENCY_IRRIGATION"}
```

This alert fires on every reading cycle while the threshold is exceeded. In production, the Gateway receives it and routes to the AI Engine for immediate action.

---

### Scenario 5 — Close valve manually

With valve OPEN from Scenario 2, send:

```json
{"packet_type":"VALVE_COMMAND","command_id":"CMD-CLOSE-001","issued_by":"AI_ENGINE","zone_id":"zone_a","device_id":"MCU-ZONE-A-001","command":"CLOSE_VALVE","duration_minutes":0,"priority":"NORMAL","linked_decision_id":"DEC-002","reason":"Manual close"}
```

```
[TX:COMMAND_ACK | ...]
{"status":"EXECUTED","valve_state_after":"CLOSED",
 "message":"Valve closed. Reason: REMOTE_COMMAND. Open for 23s."}
```

---

### Scenario 6 — Wrong node targeting (silently ignored)

Send a command targeting a different zone:

```json
{"packet_type":"VALVE_COMMAND","command_id":"CMD-WRONG","issued_by":"AI_ENGINE","zone_id":"zone_b","device_id":"MCU-ZONE-A-001","command":"OPEN_VALVE","duration_minutes":5,"priority":"NORMAL","linked_decision_id":"DEC-003","reason":"test"}
```

**Nothing happens.** No ACK, no valve action, no output. The firmware silently discards packets not addressed to it. This is correct behaviour — in a multi-node field, all nodes hear every LoRa broadcast, but only the targeted one acts.

---


*TAZROUT Smart Irrigation System — KHENFRI Moussa — March 2026*
