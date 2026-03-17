/*
 * TAZROUT Gateway LoRa Listener
 * Flash this to a second ESP32 + SX1276 module.
 *
 * What it does:
 *   - Listens for every LoRa packet from the sensor node
 *   - Decodes the framing (header + JSON + CRC)
 *   - Verifies the CRC
 *   - Prints the packet type, RSSI, SNR, and JSON payload
 *   - Lets you type a command in Serial Monitor to send a VALVE_COMMAND back
 *
 * Wiring (same as sensor node):
 *   VCC  -> 3.3V    SCK  -> GPIO 18
 *   GND  -> GND     MISO -> GPIO 19
 *   NSS  -> GPIO 5  MOSI -> GPIO 23
 *   RST  -> GPIO 14
 *   DIO0 -> GPIO 2
 *
 * LoRa parameters MUST match the sensor node exactly.
 * If any parameter differs, the two radios cannot hear each other.
 *
 * Serial commands (type in Serial Monitor):
 *   open <minutes>   Send OPEN_VALVE command to the node
 *   close            Send CLOSE_VALVE command to the node
 *   ping             Send a device state request
 *   stats            Show packet statistics
 */

#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

#define LORA_PIN_CS    5
#define LORA_PIN_RST  14
#define LORA_PIN_DIO0  2
#define LORA_FREQUENCY 868E6
#define LORA_SF        7
#define LORA_BW        125E3
#define LORA_CR        5
#define LORA_POWER     17

// These must also match the node firmware
#define PROTOCOL_VERSION  0x01
#define MAX_PACKET_BYTES  220
#define TARGET_ZONE_ID    "zone_a"
#define TARGET_DEVICE_ID  "MCU-ZONE-A-001"

// Packet type identifiers
#define PKT_SENSOR_READING   0x01
#define PKT_COMMAND_ACK      0x02
#define PKT_DEVICE_STATE     0x03
#define PKT_EMERGENCY_ALERT  0x04
#define PKT_VALVE_COMMAND    0x05

// Statistics
uint32_t packets_received = 0;
uint32_t crc_errors       = 0;
uint32_t commands_sent    = 0;
uint32_t cmd_counter      = 0;


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


void setup() {
    Serial.begin(115200);
    delay(500);

    LoRa.setPins(LORA_PIN_CS, LORA_PIN_RST, LORA_PIN_DIO0);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("LoRa init FAILED. Check wiring.");
        Serial.println("  CS=GPIO5, RST=GPIO14, DIO0=GPIO2");
        Serial.println("  Module must be 868 MHz version.");
        while (true) delay(1000);
    }

    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setTxPower(LORA_POWER);

    Serial.println();
    Serial.println("TAZROUT Gateway Listener");
    Serial.printf( "Listening on 868 MHz | SF%d | BW %.0f kHz | CR 4/%d\n",
        LORA_SF, LORA_BW / 1000.0, LORA_CR);
    Serial.println("Waiting for packets from sensor node...");
    Serial.println("Commands: open <minutes> | close | ping | stats");
    Serial.println();
}


void loop() {
    // Check for incoming radio packets
    int packet_size = LoRa.parsePacket();
    if (packet_size >= 7) {
        receive_and_decode(packet_size);
    }

    // Check for Serial commands from the user
    handle_serial_input();
}


void receive_and_decode(int packet_size) {
    // Read 4-byte header
    uint8_t len_hi   = LoRa.read();
    uint8_t len_lo   = LoRa.read();
    uint8_t pkt_type = LoRa.read();
    uint8_t version  = LoRa.read();

    // Read JSON payload, leaving last 2 bytes for CRC
    char json[MAX_PACKET_BYTES + 1];
    int json_len = 0;
    while (LoRa.available() > 2) {
        if (json_len < MAX_PACKET_BYTES) {
            json[json_len++] = (char)LoRa.read();
        } else {
            LoRa.read();
        }
    }
    json[json_len] = '\0';

    // Read and verify CRC
    uint8_t  crc_hi   = LoRa.read();
    uint8_t  crc_lo   = LoRa.read();
    uint16_t rx_crc   = ((uint16_t)crc_hi << 8) | crc_lo;
    uint16_t calc_crc = crc16((const uint8_t*)json, json_len);

    int   rssi = LoRa.packetRssi();
    float snr  = LoRa.packetSnr();

    if (rx_crc != calc_crc) {
        crc_errors++;
        Serial.printf("[CRC ERROR] rx=0x%04X calc=0x%04X | RSSI=%ddBm | SNR=%.1fdB\n",
            rx_crc, calc_crc, rssi, snr);
        Serial.println("  Packet discarded. Possible cause: interference, distance, or mismatched parameters.");
        return;
    }

    packets_received++;

    // Decode packet type name for display
    const char* type_name = "UNKNOWN";
    switch (pkt_type) {
        case PKT_SENSOR_READING:  type_name = "SENSOR_READING";  break;
        case PKT_COMMAND_ACK:     type_name = "COMMAND_ACK";     break;
        case PKT_DEVICE_STATE:    type_name = "DEVICE_STATE";    break;
        case PKT_EMERGENCY_ALERT: type_name = "EMERGENCY_ALERT"; break;
    }

    Serial.println();
    Serial.printf("[RECEIVED] %s\n", type_name);
    Serial.printf("  RSSI : %d dBm", rssi);

    // Signal quality interpretation — helps during range testing
    if      (rssi > -70)  Serial.print("  (Excellent)");
    else if (rssi > -90)  Serial.print("  (Good)");
    else if (rssi > -110) Serial.print("  (Fair)");
    else                  Serial.print("  (Weak — consider reducing distance or changing SF)");

    Serial.printf("\n  SNR  : %.1f dB", snr);
    if (snr > 10)        Serial.print("  (Very clear)");
    else if (snr > 0)    Serial.print("  (Good)");
    else if (snr > -10)  Serial.print("  (Marginal)");
    else                 Serial.print("  (Poor — packet loss likely)");

    Serial.printf("\n  Size : %d bytes\n", json_len);
    Serial.printf("  CRC  : 0x%04X OK\n", rx_crc);
    Serial.printf("  Proto: v0x%02X\n", version);
    Serial.println("  JSON :");
    Serial.println(json);

    // Pretty-print specific packet types for easier reading
    StaticJsonDocument<400> doc;
    auto err = deserializeJson(doc, json);
    if (!err) {
        String pkt = doc["pkt"] | "";
        if (pkt == "SR") {
            Serial.println("  Decoded:");
            Serial.printf("    Temperature  : %s C (%s)\n",
                (const char*)doc["t"], (const char*)doc["ts"]);
            Serial.printf("    Soil moisture: %s g/m3 (%s)\n",
                (const char*)doc["m"], (const char*)doc["ms"]);
            Serial.printf("    Water tank   : %s %% (%s)\n",
                (const char*)doc["w"], (const char*)doc["ws"]);
            Serial.printf("    Humidity     : %s %% (%s)\n",
                (const char*)doc["h"], (const char*)doc["hs"]);
        } else if (pkt == "ACK") {
            Serial.printf("  Decoded: command=%s  status=%s  valve=%s\n",
                (const char*)doc["cid"],
                (const char*)doc["s"],
                (const char*)doc["v"]);
            Serial.printf("  Message: %s\n", (const char*)doc["msg"]);
        } else if (pkt == "STATE") {
            Serial.printf("  Decoded: %s -> %s  valve=%s  fw=%s\n",
                (const char*)doc["p"],
                (const char*)doc["c"],
                (const char*)doc["v"],
                (const char*)doc["fw"]);
            const char* why = doc["why"] | "";
            if (strlen(why) > 0) Serial.printf("  Reason : %s\n", why);
        } else if (pkt == "ALERT") {
            Serial.printf("  ALERT  : %s\n", (const char*)doc["msg"]);
            Serial.printf("  Action : %s\n", (const char*)doc["act"]);
        }
    }
    Serial.println();
}


void send_valve_command(const char* command, int duration_minutes) {
    cmd_counter++;
    char cmd_id[24];
    snprintf(cmd_id, sizeof(cmd_id), "CMD-GW-%05lu", cmd_counter);

    StaticJsonDocument<200> doc;
    doc["pkt"]     = "CMD";
    doc["z"]       = TARGET_ZONE_ID;
    doc["d"]       = TARGET_DEVICE_ID;
    doc["cid"]     = cmd_id;
    doc["command"] = command;
    doc["dur"]     = duration_minutes;

    char json[MAX_PACKET_BYTES + 1];
    serializeJson(doc, json, sizeof(json));

    uint16_t json_len = strlen(json);
    uint16_t checksum = crc16((const uint8_t*)json, json_len);

    LoRa.beginPacket();
    LoRa.write((uint8_t)(json_len >> 8));
    LoRa.write((uint8_t)(json_len & 0xFF));
    LoRa.write(PKT_VALVE_COMMAND);
    LoRa.write(PROTOCOL_VERSION);
    LoRa.write((const uint8_t*)json, json_len);
    LoRa.write((uint8_t)(checksum >> 8));
    LoRa.write((uint8_t)(checksum & 0xFF));

    if (LoRa.endPacket()) {
        commands_sent++;
        Serial.printf("[SENT] %s | id=%s | dur=%dmin | %u bytes | crc=0x%04X\n",
            command, cmd_id, duration_minutes, json_len, checksum);
        Serial.println("  Waiting for COMMAND_ACK from node...");
    } else {
        Serial.println("[SEND FAILED] LoRa endPacket returned 0. Check wiring.");
    }
}


void handle_serial_input() {
    if (!Serial.available()) return;

    char line[64];
    size_t n = Serial.readBytesUntil('\n', line, sizeof(line) - 1);
    if (!n) return;
    line[n] = '\0';

    String input = String(line);
    input.trim();
    input.toLowerCase();

    if (input.startsWith("open")) {
        int minutes = 0;
        if (input.length() > 5) {
            minutes = input.substring(5).toInt();
        }
        send_valve_command("OPEN_VALVE", minutes);

    } else if (input == "close") {
        send_valve_command("CLOSE_VALVE", 0);

    } else if (input == "ping") {
        // Send a DEVICE_STATE request — node will reply with its current state
        Serial.println("[INFO] Ping not a defined downlink packet. Sending OPEN then CLOSE with 0 duration instead.");
        Serial.println("[INFO] To check if node is alive, just watch for the next SENSOR_READING.");

    } else if (input == "stats") {
        Serial.println("[STATS]");
        Serial.printf("  Packets received : %lu\n", packets_received);
        Serial.printf("  CRC errors       : %lu\n", crc_errors);
        Serial.printf("  Commands sent    : %lu\n", commands_sent);
        if (packets_received + crc_errors > 0) {
            float loss_rate = 100.0f * crc_errors / (packets_received + crc_errors);
            Serial.printf("  CRC error rate   : %.1f%%\n", loss_rate);
        }

    } else if (input.length() > 0) {
        Serial.println("Commands: open <minutes> | close | stats");
    }
}
