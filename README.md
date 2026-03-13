# TAZROUT ESP32 Sensor Node
**IoT Smart Irrigation System — Field Sensor & Control Layer**

> Part of the TAZROUT multi-disciplinary project.
> This repository covers the ESP32 embedded module only.

---

## What This Module Does

The ESP32 is the field-deployed edge node in the TAZROUT irrigation system. It sits in the soil, reads sensors every 30 seconds, and waits for commands from the AI Engine.

**It does not make irrigation decisions.** That responsibility belongs to the AI Engine upstream. The ESP32's only job is to report what it measures and execute what it is told.

```
Sensors → ESP32 → LoRa Radio → Gateway → MQTT Broker → AI Engine
                                                            ↓
Relay / Valve ← ESP32 ← LoRa Radio ← Gateway ← MQTT Broker ←
```

---

## Developer

| | |
|---|---|
| **Name** | Moussa KHENFRI |
| **Role** | ESP32 Sensor Node Developer |
| **Duration** | 8 weeks (Feb 1 – Mar 28, 2026) |
| **Contact** | moussakhanfri04@gmail.com |

---

## Hardware

| Component | GPIO | Notes |
|---|---|---|
| DHT22 (temp + humidity) | 16 | 10kΩ pull-up to 3.3V |
| Capacitive soil moisture | 35 | ADC1_CH7, inverted output |
| Water tank level | 34 | ADC1_CH6, analog 0–3.3V |
| Valve relay | 26 | Active HIGH, 5V coil |
| Status LED | 2 | Heartbeat — future LoRa DIO0 |
| LED DRY | 13 | Red |
| LED OPTIMAL | 12 | Yellow |
| LED WET | 14 | Green — future LoRa RST |

---

## Communication Architecture

The ESP32 communicates **exclusively via LoRa radio**. It has no WiFi or MQTT connection.

| Segment | Protocol | Medium |
|---|---|---|
| ESP32 → LoRa module | SPI | Wires |
| LoRa module → Gateway | LoRa 868 MHz | Radio |
| Gateway → MQTT Broker | MQTT over TCP/IP | WiFi / Ethernet |

The Gateway (Raspberry Pi + LoRa HAT) is the bridge between the radio world and the network world. The ESP32 knows nothing about MQTT.

---

## Packet Types

Five JSON packet types defined in the Technical Specification v1.0:

| Packet | Direction | When |
|---|---|---|
| `SENSOR_READING` | ESP32 → Gateway | Every 30 seconds |
| `COMMAND_ACK` | ESP32 → Gateway | After every valve command |
| `DEVICE_STATE_CHANGE` | ESP32 → Gateway | On boot, fault, watchdog |
| `EMERGENCY_ALERT` | ESP32 → Gateway | On threshold breach (debounced) |
| `VALVE_COMMAND` | Gateway → ESP32 | On AI irrigation decision |

---

## Technologies

| Category | Details |
|---|---|
| Microcontroller | ESP32-WROOM-32, dual-core 240 MHz, 4MB Flash |
| Framework | Arduino (ESP-IDF underneath) |
| Sensors | DHT22, Capacitive Soil Moisture v2, HC-SR04 |
| Communication | LoRa SX1276 — 868 MHz, SF7, BW 125 kHz, 17 dBm |
| Simulation | Wokwi ESP32 Simulator |
| Languages | C / C++ |
| Libraries | `DHT`, `ArduinoJson` v6.21+, `LoRa` by Sandeep Mistry, ESP-IDF `driver/adc.h` |

---

## Repository Structure

```
tazrout-esp32-module/
│
├── src/
│   ├── week1/          # GPIO, ADC, PWM, state machine sketches
│   ├── week2/          # Sensor integration sketches
│   └── week3/          # Production firmware v3.1 + Wokwi simulation
│       
│       
│       
│
├── docs/
│   ├── week1/          # Day reports
│   ├── week2/          # week 2 technical reports
│   ├── week3/          # Week 3 technical report
│   └── TAZROUT-ESP32-Development_Plan.pdf
│
├── guides/
│   ├── TAZROUT_HardwareTestGuide.md   # 9-phase real hardware test procedure
│   └── TAZROUT_SimulationGuide.md     # Wokwi simulation walkthrough
│
└── PROGRESS.md
```



## Documentation

- [Development Plan](docs/TAZROUT-ESP32-Development_Plan.pdf)
- [Progress Tracker](docs/PROGRESS.md)
- [Hardware Test Guide](docs/guides/TAZROUT_HardwareTestGuide.md)
- [Simulation Guide](docs/guides/TAZROUT_SimulationGuide.md)
- API Documentation — coming in Week 5

---

## License

MIT License

---

*Last updated: March 2026
