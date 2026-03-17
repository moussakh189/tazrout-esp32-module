"""
TAZROUT Smart Irrigation System
Python Gateway

Developer : KHENFRI Moussa

Purpose:
    Bridge between the ESP32 LoRa sensor nodes and the MQTT broker.
    Receives sensor data from the field and forwards irrigation commands back.

Two modes — change MODE at line 37:

    SERIAL  (use now, on your PC, USB cable to ESP32, no LoRa hardware needed)
        Connects to the ESP32 via USB.
        Reads the JSON packets the firmware prints for debugging.
        Expands compact keys, publishes to MQTT exactly like the real gateway.
        Useful for testing packet format, valve commands, and MQTT topics
        before LoRa hardware arrives.

    LORA  (use on Raspberry Pi when hardware is ready)
        Receives real LoRa radio packets from the sensor node.
        Validates CRC, expands compact JSON, publishes to MQTT broker.
        Subscribes to MQTT valve command topic, forwards to node via LoRa.

Compact key mapping (LoRa radio uses short keys to stay under 255 bytes):
    pkt:SR    -> SENSOR_READING       pkt:ACK   -> COMMAND_ACK
    pkt:STATE -> DEVICE_STATE_CHANGE  pkt:ALERT -> EMERGENCY_ALERT
    pkt:CMD   -> VALVE_COMMAND (sent TO the node)
    z/d -> zone_id/device_id
    t/m/w/h -> temperature/soil_moisture/water_level/humidity values
    ts/ms/ws/hs -> sensor status fields
    cid -> command_id   s -> status   v -> valve_state
    p/c -> previous/current device state   why -> reason
    id -> alert_id   act -> recommended_action   dur -> duration_minutes

Dependencies:
    pip install pyserial paho-mqtt

    LoRa mode on Raspberry Pi only:
        sudo raspi-config -> Interface Options -> SPI -> Enable
        pip install RPi.GPIO spidev
        pip install git+https://github.com/rpsreal/pySX127x
"""

import json
import sys
import threading
import time
from datetime import datetime, timezone

import serial
import serial.tools.list_ports
import paho.mqtt.client as mqtt


MODE = "SERIAL"  # "SERIAL" for USB testing  |  "LORA" for Raspberry Pi

SERIAL_PORT = "COM3"  # Windows: COM3  Linux: /dev/ttyUSB0  Mac: /dev/cu.usbserial-...
SERIAL_BAUDRATE = 115200

LORA_FREQUENCY = 868.0  # MHz — must match ESP32 firmware exactly
LORA_SF = 7
LORA_BW = 125.0  # kHz
LORA_CR = 5
LORA_POWER = 14  # dBm — Raspberry Pi HAT uses 14, ESP32 uses 17

MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_USERNAME = ""
MQTT_PASSWORD = ""

TOPICS = {
    "SR": "tazrout/zones/{zone}/sensors",
    "ACK": "tazrout/zones/{zone}/valve/ack",
    "STATE": "tazrout/zones/{zone}/state",
    "ALERT": "tazrout/zones/{zone}/alert",
    "CMD": "tazrout/zones/{zone}/valve/command",
}

PROTOCOL_VERSION = 0x01
MAX_PACKET_BYTES = 220
PKT_VALVE_COMMAND = 0x05


def crc16_xmodem(data: bytes) -> int:
    """
    CRC-16/XMODEM: polynomial 0x1021, initial value 0x0000.
    Must produce identical results to the C++ crc16() in TAZROUT_Node_v4.ino.
    """
    crc = 0x0000
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if (crc & 0x8000) else crc << 1
        crc &= 0xFFFF
    return crc


def _expand_status(short: str) -> str:
    return {"OK": "OK", "OOR": "OUT_OF_RANGE", "FAULT": "SENSOR_FAULT"}.get(
        short, short
    )


def expand_packet(compact: dict, rssi: int = 0, snr: float = 0.0) -> dict:
    """
    Convert compact LoRa JSON to full spec format for MQTT.
    The Gateway adds the timestamp (ESP32 has no real-time clock).
    The Gateway injects RSSI and SNR which only the receiver can measure.
    """
    pkt = compact.get("pkt", "")
    zone = compact.get("z", "unknown")
    dev = compact.get("d", "unknown")
    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    if pkt == "SR":
        return {
            "packet_type": "SENSOR_READING",
            "zone_id": zone,
            "device_id": dev,
            "timestamp": ts,
            "sensors": {
                "temperature": {
                    "value": compact.get("t"),
                    "unit": "C",
                    "status": _expand_status(compact.get("ts", "OK")),
                },
                "soil_moisture": {
                    "value": compact.get("m"),
                    "unit": "g/m3",
                    "status": _expand_status(compact.get("ms", "OK")),
                },
                "water_level": {
                    "value": compact.get("w"),
                    "unit": "%",
                    "status": _expand_status(compact.get("ws", "OK")),
                },
                "humidity": {
                    "value": compact.get("h"),
                    "unit": "%",
                    "status": _expand_status(compact.get("hs", "OK")),
                },
            },
            "signal": {"rssi": rssi, "snr": snr, "spreading_factor": LORA_SF},
        }

    if pkt == "ACK":
        return {
            "packet_type": "COMMAND_ACK",
            "zone_id": zone,
            "device_id": dev,
            "command_id": compact.get("cid"),
            "timestamp": ts,
            "status": compact.get("s"),
            "valve_state_after": compact.get("v"),
            "message": compact.get("msg"),
        }

    if pkt == "STATE":
        result = {
            "packet_type": "DEVICE_STATE_CHANGE",
            "zone_id": zone,
            "device_id": dev,
            "timestamp": ts,
            "previous_device_state": compact.get("p"),
            "current_device_state": compact.get("c"),
            "valve_state": compact.get("v"),
            "firmware_version": compact.get("fw"),
            "battery_level": compact.get("bat", -1),
        }
        if compact.get("why"):
            result["reason"] = compact["why"]
        return result

    if pkt == "ALERT":
        return {
            "packet_type": "EMERGENCY_ALERT",
            "zone_id": zone,
            "device_id": dev,
            "alert_id": compact.get("id"),
            "timestamp": ts,
            "severity": "CRITICAL",
            "triggered_by": "SENSOR_THRESHOLD",
            "sensor_values": {
                "temperature": compact.get("t"),
                "soil_moisture": compact.get("m"),
                "water_level": compact.get("w"),
                "humidity": compact.get("h"),
            },
            "message": compact.get("msg"),
            "recommended_action": compact.get("act"),
        }

    return compact


def build_valve_command(
    zone: str, device: str, command: str, duration_minutes: int
) -> dict:
    """Build a compact VALVE_COMMAND packet to send to the sensor node."""
    return {
        "pkt": "CMD",
        "z": zone,
        "d": device,
        "cid": f"CMD-GW-{int(time.time())}",
        "command": command,
        "dur": duration_minutes,
    }


class MQTTClient:
    """Thin wrapper around paho-mqtt with auto-reconnect."""

    def __init__(self, on_command_callback=None):
        self.client = mqtt.Client(client_id="tazrout-gateway")
        self.connected = False
        self.on_command_callback = on_command_callback

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

        if MQTT_USERNAME:
            self.client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

    def connect(self):
        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            self.client.loop_start()
            time.sleep(1.0)
        except Exception as e:
            print(f"MQTT broker not reachable ({MQTT_BROKER}:{MQTT_PORT}): {e}")
            print("Packets will be printed to console but not published to MQTT.")

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print(f"MQTT connected to {MQTT_BROKER}:{MQTT_PORT}")
            client.subscribe("tazrout/zones/+/valve/command", qos=1)
        else:
            print(f"MQTT connection refused (code {rc})")

    def _on_disconnect(self, client, userdata, rc):
        self.connected = False
        if rc != 0:
            print("MQTT disconnected unexpectedly.")

    def _on_message(self, client, userdata, message):
        """Called when a VALVE_COMMAND arrives from the AI Engine via MQTT."""
        try:
            payload = json.loads(message.payload.decode())
            print(f"\nMQTT command on {message.topic}")
            if self.on_command_callback:
                self.on_command_callback(payload)
        except Exception as e:
            print(f"MQTT message error: {e}")

    def publish(self, topic: str, payload: dict, pkt_type: str = ""):
        qos = 2 if pkt_type == "ALERT" else 1
        retain = pkt_type == "STATE"
        body = json.dumps(payload)

        if self.connected:
            self.client.publish(topic, body, qos=qos, retain=retain)
            print(
                f"  MQTT -> {topic}  ({len(body)}B  QoS {qos}{'  retain' if retain else ''})"
            )
        else:
            print(f"  [no broker] {topic}")
            print(f"  {json.dumps(payload, indent=2)}")

    def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()


class GatewayBase:
    """
    Logic shared between Serial and LoRa modes.
    Receives a decoded compact JSON dict, expands it, and publishes to MQTT.
    """

    def __init__(self):
        self.mqtt = MQTTClient(on_command_callback=self._on_mqtt_command)
        self.mqtt.connect()
        self.packets_received = 0
        self.crc_errors = 0
        self.commands_sent = 0

    def on_packet(self, compact: dict, rssi: int = 0, snr: float = 0.0):
        """Process one incoming packet from the sensor node."""
        pkt = compact.get("pkt", "?")
        dev = compact.get("d", "?")
        zone = compact.get("z", "unknown")

        self.packets_received += 1
        ts = datetime.now().strftime("%H:%M:%S")

        print(f"\n[{ts}] {pkt} from {dev}", end="")
        if rssi:
            quality = (
                "Excellent"
                if rssi > -70
                else "Good" if rssi > -90 else "Fair" if rssi > -110 else "Weak"
            )
            print(f"  RSSI {rssi} dBm ({quality})  SNR {snr:.1f} dB", end="")
        print()

        self._print_summary(compact)

        expanded = expand_packet(compact, rssi, snr)
        topic = TOPICS.get(pkt, "tazrout/unknown").format(zone=zone)
        self.mqtt.publish(topic, expanded, pkt)

    def _print_summary(self, compact: dict):
        pkt = compact.get("pkt", "")
        if pkt == "SR":
            print(
                f"  Temp {compact.get('t')} C  |  "
                f"Moisture {compact.get('m')} g/m3  |  "
                f"Water {compact.get('w')}%  |  "
                f"Humidity {compact.get('h')}%"
            )
        elif pkt == "ACK":
            print(
                f"  {compact.get('cid')} -> {compact.get('s')} | Valve: {compact.get('v')}"
            )
            print(f"  {compact.get('msg', '')}")
        elif pkt == "STATE":
            print(
                f"  {compact.get('p')} -> {compact.get('c')} | Valve: {compact.get('v')}"
            )
            if compact.get("why"):
                print(f"  Reason: {compact.get('why')}")
        elif pkt == "ALERT":
            print(f"  ALERT: {compact.get('msg')}")
            print(f"  Action: {compact.get('act')}")

    def _on_mqtt_command(self, full_command: dict):
        """
        Called when a VALVE_COMMAND from the AI Engine arrives on MQTT.
        Subclasses override this to forward the command to the node via their transport.
        """
        pass

    def print_stats(self):
        print(f"\nStatistics:")
        print(f"  Received : {self.packets_received}")
        print(f"  CRC errors: {self.crc_errors}")
        print(f"  Sent     : {self.commands_sent}")


class SerialGateway(GatewayBase):
    """
    Serial mode: USB cable between PC and ESP32.
    Reads debug JSON output from the ESP32 firmware.
    Forwards valve commands by writing compact JSON to the serial port.
    """

    def __init__(self):
        super().__init__()
        self.ser = None
        self.running = False
        self.zone_id = "zone_a"
        self.device_id = "MCU-ZONE-A-001"

    def connect(self) -> bool:
        try:
            self.ser = serial.Serial(SERIAL_PORT, SERIAL_BAUDRATE, timeout=1)
            time.sleep(2)
            print(f"Serial: {SERIAL_PORT}  {SERIAL_BAUDRATE} baud")
            return True
        except serial.SerialException as e:
            print(f"Cannot open {SERIAL_PORT}: {e}")
            print()
            print("Available ports:")
            for p in serial.tools.list_ports.comports():
                print(f"  {p.device}  {p.description}")
            print()
            print(f"Change SERIAL_PORT at the top of this file.")
            return False

    def _read_loop(self):
        buffer = ""
        while self.running:
            try:
                if self.ser and self.ser.in_waiting:
                    chunk = self.ser.read(self.ser.in_waiting).decode(
                        "utf-8", errors="replace"
                    )
                    buffer += chunk
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        line = line.strip()
                        if line.startswith("{"):
                            try:
                                self.on_packet(json.loads(line))
                            except json.JSONDecodeError:
                                print(f"  [parse error] {line[:80]}")
                        elif line:
                            print(f"  [node] {line}")
            except Exception as e:
                if self.running:
                    print(f"  [read error] {e}")
            time.sleep(0.02)

    def send(self, command: str, duration_minutes: int):
        packet = build_valve_command(
            self.zone_id, self.device_id, command, duration_minutes
        )
        if self.ser:
            self.ser.write((json.dumps(packet) + "\n").encode())
            self.commands_sent += 1
            print(f"  Sent: {json.dumps(packet)}")

    def _on_mqtt_command(self, full_command: dict):
        """Forward an MQTT valve command to the ESP32 via Serial."""
        cmd = full_command.get("command", "CLOSE_VALVE")
        duration = full_command.get("duration_minutes", 0)
        self.send(cmd, duration)

    def run(self):
        if not self.connect():
            return

        self.running = True
        threading.Thread(target=self._read_loop, daemon=True).start()

        print()
        print("Commands: open <minutes> | close | ping | stats | quit")
        print()

        while True:
            try:
                parts = input("> ").strip().lower().split()
            except (EOFError, KeyboardInterrupt):
                break

            if not parts:
                continue

            if parts[0] == "open":
                self.send("OPEN_VALVE", int(parts[1]) if len(parts) > 1 else 0)
            elif parts[0] == "close":
                self.send("CLOSE_VALVE", 0)
            elif parts[0] == "ping" and self.ser:
                self.ser.write(b"ping\n")
            elif parts[0] == "stats":
                self.print_stats()
            elif parts[0] == "quit":
                break
            else:
                print("Commands: open <minutes> | close | ping | stats | quit")

        self.running = False
        if self.ser:
            self.ser.close()
        self.mqtt.disconnect()


class LoRaGateway(GatewayBase):
    """
    LoRa mode: Raspberry Pi with SX1276 HAT.
    Receives real radio packets, publishes to MQTT.
    Forwards MQTT valve commands back to node via radio.

    Requires:
        sudo raspi-config -> Interface Options -> SPI -> Enable
        pip install RPi.GPIO spidev
        pip install git+https://github.com/rpsreal/pySX127x
    """

    def __init__(self):
        super().__init__()
        self.lora = None
        self.running = False

    def _init_radio(self) -> bool:
        try:
            from SX127x.LoRa import LoRa
            from SX127x.board_config import BOARD
            from SX127x.constants import BW, CODING_RATE, MODE

            BOARD.setup()
            self.lora = LoRa(verbose=False)
            self.lora.set_mode(MODE.SLEEP)
            self.lora.set_freq(LORA_FREQUENCY)
            self.lora.set_spreading_factor(LORA_SF)
            self.lora.set_bw(BW.BW125)
            self.lora.set_coding_rate(CODING_RATE.CR4_5)
            self.lora.set_pa_config(pa_select=1, output_power=LORA_POWER)
            self.lora.reset_ptr_rx()
            self.lora.set_mode(MODE.RXCONT)

            print(
                f"LoRa ready: {LORA_FREQUENCY} MHz | SF{LORA_SF} | BW{LORA_BW} kHz | {LORA_POWER} dBm"
            )
            return True

        except ImportError:
            print("pySX127x not installed.")
            print("Run: pip install git+https://github.com/rpsreal/pySX127x")
            return False
        except Exception as e:
            print(f"LoRa init failed: {e}")
            return False

    def _read_one_packet(self):
        """
        Read and decode one LoRa packet.
        Returns (compact_dict, rssi, snr) or (None, 0, 0) on error.
        """
        payload = self.lora.read_payload(nocheck=True)
        if not payload or len(payload) < 7:
            return None, 0, 0.0

        json_len = (payload[0] << 8) | payload[1]
        # payload[2] = packet type, payload[3] = protocol version

        if len(payload) < 4 + json_len + 2:
            print(
                f"  [error] Short packet: {len(payload)} bytes, expected {4 + json_len + 2}"
            )
            return None, 0, 0.0

        json_bytes = bytes(payload[4 : 4 + json_len])
        rx_crc = (payload[4 + json_len] << 8) | payload[4 + json_len + 1]
        calc_crc = crc16_xmodem(json_bytes)
        rssi = self.lora.get_pkt_rssi_value()
        snr = self.lora.get_pkt_snr_value()

        if rx_crc != calc_crc:
            self.crc_errors += 1
            print(
                f"  [CRC error] rx=0x{rx_crc:04X} calc=0x{calc_crc:04X} | RSSI {rssi} dBm"
            )
            return None, rssi, snr

        try:
            return json.loads(json_bytes.decode("utf-8")), rssi, snr
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            print(f"  [decode error] {e}")
            return None, rssi, snr

    def _send_to_node(self, packet: dict):
        """Transmit a compact JSON packet to the sensor node via LoRa."""
        from SX127x.constants import MODE

        json_bytes = json.dumps(packet).encode("utf-8")
        json_len = len(json_bytes)
        checksum = crc16_xmodem(json_bytes)

        frame = bytearray()
        frame += bytes(
            [json_len >> 8, json_len & 0xFF, PKT_VALVE_COMMAND, PROTOCOL_VERSION]
        )
        frame += json_bytes
        frame += bytes([checksum >> 8, checksum & 0xFF])

        self.lora.set_mode(MODE.STDBY)
        self.lora.write_payload(list(frame))
        self.lora.set_mode(MODE.TX)
        time.sleep(0.5)
        self.lora.reset_ptr_rx()
        self.lora.set_mode(MODE.RXCONT)

        self.commands_sent += 1
        print(f"  LoRa TX: {json.dumps(packet)} ({len(frame)} bytes)")

    def _on_mqtt_command(self, full_command: dict):
        """Convert full MQTT format to compact and send via LoRa."""
        compact = build_valve_command(
            zone=full_command.get("zone_id", "zone_a"),
            device=full_command.get("device_id", "MCU-ZONE-A-001"),
            command=full_command.get("command", "CLOSE_VALVE"),
            duration_minutes=full_command.get("duration_minutes", 0),
        )
        compact["cid"] = full_command.get("command_id", compact["cid"])
        self._send_to_node(compact)

    def run(self):
        if not self._init_radio():
            return

        self.running = True
        print("LoRa Gateway running. Press Ctrl+C to stop.")

        try:
            while self.running:
                compact, rssi, snr = self._read_one_packet()
                if compact:
                    self.on_packet(compact, rssi, snr)
                time.sleep(0.05)

        except KeyboardInterrupt:
            print("\nStopping.")

        finally:
            from SX127x.constants import MODE
            from SX127x.board_config import BOARD

            self.lora.set_mode(MODE.SLEEP)
            BOARD.teardown()
            self.mqtt.disconnect()
            self.print_stats()


def main():
    print()
    print(f"TAZROUT Gateway  |  Mode: {MODE}  |  MQTT: {MQTT_BROKER}:{MQTT_PORT}")
    print()

    if MODE == "SERIAL":
        SerialGateway().run()
    elif MODE == "LORA":
        LoRaGateway().run()
    else:
        print(f"Unknown MODE: '{MODE}'. Set to 'SERIAL' or 'LORA'.")
        sys.exit(1)


if __name__ == "__main__":
    main()
