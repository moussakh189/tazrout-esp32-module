# TAZROUT IoT Irrigation System
## JSON Data Format Specification v1.0

**Document Version:** 1.0
**Last Updated:** February 14, 2026
**Author:** KHENFRI Moussa (ESP32 Module)
**Status:** Final - Ready for Integration

---

## 1. Overview

This document defines the JSON data format used by the TAZROUT ESP32 sensor node for transmitting sensor readings and system status via MQTT protocol.

**Purpose:**
- Standardize data exchange between ESP32 node and backend/UI/AI systems
- Ensure compatibility across all TAZROUT components
- Provide clear contract for team integration

**Protocol:** MQTT
**Encoding:** UTF-8
**Max Message Size:** 512 bytes

---

## 2. Complete JSON Structure

### 2.1 Full Message Format

```json
{
  "node_id": "TAZROUT_ESP32_001",
  "timestamp": 1707926400000,
  "reading_number": 42,
  "sensors": {
    "moisture": {
      "value": 45,
      "status": "OPTIMAL",
      "unit": "percent",
      "valid": true
    },
    "temperature": {
      "value": 24.5,
      "unit": "celsius",
      "valid": true
    },
    "humidity": {
      "value": 62.0,
      "unit": "percent",
      "valid": true
    }
  },
  "irrigation": {
    "pump_active": false,
    "threshold": 35,
    "mode": "auto",
    "last_activation": 1707922800000
  },
  "system": {
    "uptime_ms": 3600000,
    "buffer_count": 10,
    "wifi_rssi": -67,
    "free_heap": 234567
  }
}
```

---

## 3. Field Specifications

### 3.1 Root Level Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `node_id` | String | Yes | Unique identifier for this ESP32 node |
| `timestamp` | Number | Yes | Unix timestamp in milliseconds (UTC) |
| `reading_number` | Number | Yes | Sequential reading counter (increments each reading) |
| `sensors` | Object | Yes | All sensor readings |
| `irrigation` | Object | Yes | Irrigation system status |
| `system` | Object | Yes | System health and diagnostics |

---

### 3.2 Sensors Object

#### 3.2.1 Moisture Sensor

```json
"moisture": {
  "value": 45,
  "status": "OPTIMAL",
  "unit": "percent",
  "valid": true
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `value` | Number | 0-100 | Soil moisture percentage |
| `status` | String | See below | Human-readable status |
| `unit` | String | "percent" | Measurement unit (fixed) |
| `valid` | Boolean | true/false | Data validity flag |

**Status Values:**
- `"DRY"` - Moisture < 30%
- `"OPTIMAL"` - Moisture 30-70%
- `"WET"` - Moisture > 70%

#### 3.2.2 Temperature Sensor

```json
"temperature": {
  "value": 24.5,
  "unit": "celsius",
  "valid": true
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `value` | Number | -40 to 80 | Air temperature |
| `unit` | String | "celsius" | Measurement unit (fixed) |
| `valid` | Boolean | true/false | Data validity flag |

#### 3.2.3 Humidity Sensor

```json
"humidity": {
  "value": 62.0,
  "unit": "percent",
  "valid": true
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `value` | Number | 0-100 | Relative humidity percentage |
| `unit` | String | "percent" | Measurement unit (fixed) |
| `valid` | Boolean | true/false | Data validity flag |

---

### 3.3 Irrigation Object

```json
"irrigation": {
  "pump_active": false,
  "threshold": 35,
  "mode": "auto",
  "last_activation": 1707922800000
}
```

| Field | Type | Description |
|-------|------|-------------|
| `pump_active` | Boolean | Current pump state (true=ON, false=OFF) |
| `threshold` | Number | Current irrigation threshold (%) - weather-adjusted |
| `mode` | String | Operating mode: "auto" or "manual" |
| `last_activation` | Number | Unix timestamp of last pump activation (optional) |

---

### 3.4 System Object

```json
"system": {
  "uptime_ms": 3600000,
  "buffer_count": 10,
  "wifi_rssi": -67,
  "free_heap": 234567
}
```

| Field | Type | Description |
|-------|------|-------------|
| `uptime_ms` | Number | System uptime in milliseconds |
| `buffer_count` | Number | Number of readings in circular buffer (0-10) |
| `wifi_rssi` | Number | WiFi signal strength in dBm (Week 3) |
| `free_heap` | Number | Free memory in bytes |

---

## 4. Message Examples

### 4.1 Normal Operation (All Sensors Valid)

```json
{
  "node_id": "TAZROUT_ESP32_001",
  "timestamp": 1707926400000,
  "reading_number": 150,
  "sensors": {
    "moisture": {
      "value": 45,
      "status": "OPTIMAL",
      "unit": "percent",
      "valid": true
    },
    "temperature": {
      "value": 24.5,
      "unit": "celsius",
      "valid": true
    },
    "humidity": {
      "value": 62.0,
      "unit": "percent",
      "valid": true
    }
  },
  "irrigation": {
    "pump_active": false,
    "threshold": 35,
    "mode": "auto",
    "last_activation": 1707922800000
  },
  "system": {
    "uptime_ms": 3600000,
    "buffer_count": 10,
    "wifi_rssi": -67,
    "free_heap": 234567
  }
}
```

---

### 4.2 Dry Soil - Pump Activated

```json
{
  "node_id": "TAZROUT_ESP32_001",
  "timestamp": 1707930000000,
  "reading_number": 200,
  "sensors": {
    "moisture": {
      "value": 25,
      "status": "DRY",
      "unit": "percent",
      "valid": true
    },
    "temperature": {
      "value": 32.0,
      "unit": "celsius",
      "valid": true
    },
    "humidity": {
      "value": 35.0,
      "unit": "percent",
      "valid": true
    }
  },
  "irrigation": {
    "pump_active": true,
    "threshold": 40,
    "mode": "auto",
    "last_activation": 1707930000000
  },
  "system": {
    "uptime_ms": 7200000,
    "buffer_count": 10,
    "wifi_rssi": -65,
    "free_heap": 230000
  }
}
```

**Note:** High temperature (32°C) and low humidity (35%) caused threshold to increase from base 30% to 40%.

---

### 4.3 Sensor Error - DHT22 Failed

```json
{
  "node_id": "TAZROUT_ESP32_001",
  "timestamp": 1707933600000,
  "reading_number": 250,
  "sensors": {
    "moisture": {
      "value": 55,
      "status": "OPTIMAL",
      "unit": "percent",
      "valid": true
    },
    "temperature": {
      "value": 0,
      "unit": "celsius",
      "valid": false
    },
    "humidity": {
      "value": 0,
      "unit": "percent",
      "valid": false
    }
  },
  "irrigation": {
    "pump_active": false,
    "threshold": 30,
    "mode": "auto",
    "last_activation": 1707930000000
  },
  "system": {
    "uptime_ms": 10800000,
    "buffer_count": 10,
    "wifi_rssi": -70,
    "free_heap": 228000
  }
}
```

**Note:** When `valid: false`, the `value` field should be ignored. System falls back to base threshold (30%) when weather sensors are invalid.

---

## 5. MQTT Integration

### 5.1 Topic Structure

**Recommended Topics:**

| Topic | Direction | Description |
|-------|-----------|-------------|
| `tazrout/sensors/data` | Publish | Sensor readings (this JSON) |
| `tazrout/sensors/status` | Publish | Health/error messages |
| `tazrout/commands` | Subscribe | Commands from backend/UI |
| `tazrout/ack` | Publish | Command acknowledgments |

### 5.2 Publishing Frequency

**Normal Mode:**
- Sensor data: Every 3 seconds
- Statistics: Every 30 seconds (separate message)

**Power Save Mode (optional):**
- Sensor data: Every 30 seconds
- Statistics: Every 5 minutes

### 5.3 QoS Recommendations

- **Sensor data:** QoS 0 (at most once) - high frequency, loss acceptable
- **Irrigation status:** QoS 1 (at least once) - critical state changes
- **Commands:** QoS 1 (at least once) - ensure delivery

---

## 6. Data Types and Validation

### 6.1 Data Type Rules

| Field Type | JSON Type | Validation |
|------------|-----------|------------|
| Percentages | Number | 0 ≤ value ≤ 100 |
| Temperature | Number | -40 ≤ value ≤ 80 |
| Timestamps | Number | Unix epoch ms > 0 |
| Booleans | Boolean | true or false only |
| Strings | String | UTF-8, max 50 chars |
| Node ID | String | Format: `TAZROUT_ESP32_XXX` |

### 6.2 Required Field Validation

**Backend systems MUST check:**
1. All required root fields present
2. `valid` flags before using sensor data
3. Timestamp is reasonable (not too old/future)
4. Node ID matches expected format

---

## 7. Backwards Compatibility

### 7.1 Version Changes

**Version 1.0 (Current):**
- Initial release
- All fields defined above

**Future Versions:**
- May add optional fields (safe to ignore)
- Will NOT remove required fields
- Will NOT change field types

### 7.2 Handling Unknown Fields

Backend systems SHOULD:
- Accept messages with additional unknown fields
- Log warnings for unknown fields
- Continue processing known fields

---

## 8. Error Codes and Messages

### 8.1 Sensor Validation

When `valid: false`, possible reasons:

**Moisture Sensor:**
- Disconnected (ADC reads 0 or 4095)
- Suspicious reading (>30% change in single reading)
- Calibration failure

**DHT22 Sensor:**
- Communication timeout
- Checksum error
- NaN (Not a Number) returned
- Exceeded retry attempts (3)

### 8.2 Error Reporting

Errors should be published to `tazrout/sensors/status`:

```json
{
  "node_id": "TAZROUT_ESP32_001",
  "timestamp": 1707933600000,
  "level": "error",
  "message": "DHT22 sensor timeout after 3 retries",
  "sensor": "temperature"
}
```
---

## 9. Support and Updates

**Document Owner:** KHENFRI Moussa (ESP32 Module)
**Contact:** moussakhanfri04@gmail.com
**Last Review:** February 14, 2026
**Next Review:** March 1, 2026 (after Week 3 integration)

**Change Requests:**
Submit changes via team communication channel with:
- Reason for change
- Impact on existing integrations
- Proposed new structure
- Backwards compatibility plan

---

## Appendix A: Quick Reference

### Message Size
- Typical: ~400 bytes
- Maximum: 512 bytes
- Minimized (no whitespace): ~350 bytes

### Key Timestamps
- `timestamp`: When reading was taken
- `last_activation`: When pump was last turned ON

### Critical Fields for Irrigation
- `sensors.moisture.value`
- `sensors.moisture.valid`
- `irrigation.pump_active`
- `irrigation.threshold`

### Health Monitoring Fields
- All `valid` flags
- `system.uptime_ms`
- `system.free_heap`
- `system.wifi_rssi` (Week 3)

---

**END OF DOCUMENT**

*This specification is part of the TAZROUT IoT Irrigation System documentation suite.*
