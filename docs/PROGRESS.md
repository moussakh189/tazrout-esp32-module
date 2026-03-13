# TAZROUT — Development Progress
**Developer:** KHENFRI Moussa | **Module:** ESP32 Sensor Node

---

## Status Overview

| Week                   | Focus                               | Status     |
| ---------------------- | ----------------------------------- | ---------- |
| Week 1 (Feb 1–7)       | ESP32 Fundamentals                  |  Complete |
| Week 2 (Feb 8–15)      | Sensor Integration                  |  Complete |
| Week 3 (Feb 16–Mar 13) | Valve Control + Spec-Compliant JSON |  Complete |
| Week 4 (next)          | LoRa TX/RX Integration              |  Planned  |

---

## Week 1 — ESP32 Fundamentals ✅

| Day | Deliverable                          | Quality |
| --- | ------------------------------------ | ------- |
| 1   | LED Blink & GPIO                     | 84%     |
| 2   | Serial communication (4 formats)     | 96%     |
| 3   | State machine Part 1 (traffic light) | 97%     |
| 4   | State machine Part 2 (enhanced)      | 98%     |
| 5   | ADC + moving average filter          | 100%    |
| 6   | PWM LED control (full control loop)  | 100%    |
| 7   | Git setup + review                   | ✅       |

**Totals:** ~19.5h | ~1,410 lines | Avg quality 96%

---

## Week 2 — Sensor Integration ✅

| Day   | Deliverable                                                          |
| ----- | -------------------------------------------------------------------- |
| 8–9   | Capacitive moisture sensor: ADC, inverted calibration, filtering     |
| 10–11 | DHT22: temp + humidity, retry logic, dynamic threshold               |
| 12–13 | Circular buffer (20 readings), statistics engine, tri-LED indicators |
| 14    | Multi-sensor coordination (simulation), JSON output                  |
| 15    | `TAZROUT_Node_v1.ino` — first production firmware with valve relay   |

**Totals:** ~25h | Production firmware + full documentation

---

## Week 3 — Valve Control + Spec-Compliant JSON ✅

**Deliverable:** `TAZROUT_Node_v3.ino` (production) + `TAZROUT_Node_v3_SIM.ino` (Wokwi)


### All 5 packet types implemented:
-  `SENSOR_READING` — every 30s
-  `COMMAND_ACK` — after every valve command
-  `DEVICE_STATE_CHANGE` — on boot
-  `EMERGENCY_ALERT` — on threshold breach
-  `VALVE_COMMAND` — received + parsed from Gateway

### Valve safety :
-  Reject if water level < 5%
-  Auto-close after `duration_minutes`
-  Double-execution prevention

### LoRa transport abstracted:
- CRC-16/XMODEM implemented
-  `transmitPacket()` / `receivePacket()` ready for SX1276 swap
- **Actual SX1276 LoRa — Week 4**

---

## Week 4 — LoRa Communication 🔄 (Planned)

**Hardware needed:** SX1276 868 MHz module + antenna
**GPIO conflict:** LoRa DIO0=GPIO2, RST=GPIO14 → must reassign LED pins before wiring
**Tasks:** Wire SX1276, swap transport layer, add 3-retry logic, range test, e2e with gateway team
**After Week 4 → ESP32 module complete.**

---

## Firmware History

| Version | Date     | Key Change                                                        |
| ------- | -------- | ----------------------------------------------------------------- |
| v2.0    | Feb 15   | First production fw — sensors + valve + JSON                      |
| v3.0    | Mar 2026 | Spec-compliant — correct arch, 5 packet types, water level sensor |
| v4.0    | Week 4   | LoRa TX/RX (SX1276)                                               |

*Last updated: March 2026 — Week 3 complete*
