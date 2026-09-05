<p align="center">
  <img src="doc/meshpump-logo.png" alt="MeshPump Logo" width="320"/>
</p>

# MeshPump

**MeshPump** is a specialized embedded smart water pump and relay automation service running on Linux / Raspberry Pi platforms integrated with [Meshtastic](https://meshtastic.org) LoRa networks. It enables secure, decentralized remote water pump control (fish pond circulation and upper elevation replenishment), automated safety cut-off timers, multi-line MAX7219 LED dot-matrix message displays, environmental telemetry, and full integration with the HomeMesh smart automation ecosystem.

---

## Key Features

- **Dual Relay Water Pump Control**: Independent management of Fish Pond circulation pump and Upper pump with configurable auto-cutoff protection timers (5s–120s) to prevent dry running and flooding.
- **Automated Scheduled Lighting**: Crontab-driven scheduled lighting/relay automation (GPIO21) synchronized with local time.
- **MAX7219 LED Dot-Matrix Display**: Real-time multi-panel 8x8/8x32 LED scrolling message display supporting multi-row messages, per-row TTL expiration, adjustable scroll step delays, and per-row slowdown factors (`sf`).
- **Raspberry Pi Telemetry**: Real-time Broadcom VCIO CPU core temperature monitoring and ambient environmental telemetry.
- **HomeMesh Auto-Discovery (`identify`)**: Full compliance with the `HomeMesh` automation protocol, providing structured capability reporting (`identify: app=meshpump ver=2.1.4 hw=linux caps=pump_fish,pump_up,led,env`) for automatic entity discovery in [MeshMon](https://github.com) and Home Assistant.
- **Dual Interactive Command Shells**: Concurrent, non-blocking command-line shells on standard I/O (local terminal console) and TCP network socket for remote diagnostics and live hardware control.
- **Non-Volatile Configuration Persistence**: Atomic, file-backed storage (`~/.meshpump`) for authorized channels, encryption keys, admin/mate access control lists, and runtime preferences.

---

## Hardware Pinout Specifications

| Pin / GPIO | Function | Description |
| :--- | :--- | :--- |
| **GPIO 26** | `RELAY1_PIN` | Relay 1 output for Fish Pond circulation pump (active-low). |
| **GPIO 20** | `RELAY2_PIN` | Relay 2 output for Upper elevation water pump with auto-cutoff (active-low). |
| **GPIO 21** | `RELAY3_PIN` | Relay 3 output for Auxiliary night lighting / scheduled load (active-low). |
| **SPI0 / GPIO** | `MAX7219` | MAX7219 8x8 / 8x32 cascading LED dot-matrix display panels. |
| **UART0 / USB** | `/dev/ttyAMA0` | Hardware serial or USB CDC link to Meshtastic LoRa node (e.g. Heltec V3). |
| **TCP Port** | `4444` (Default) | Remote interactive shell over TCP network socket. |

---

## Documentation Index

The `doc/` directory contains protocol and architecture specifications:

| Document | Description |
| :--- | :--- |
| [**`doc/HomeChat-meshpump.md`**](doc/HomeChat-meshpump.md) | Full specification of `meshpump` HomeChat commands (pump relay control, safety cutoff timers, MAX7219 LED matrix, CPU temperature telemetry, and HomeMesh `identify` auto-discovery). |

---

## Building & Installation

### Prerequisites

- Linux (Debian / Raspberry Pi OS / Ubuntu)
- GCC / G++ (C++17 support)
- CMake 3.13+
- `liblgpio-dev` (Linux GPIO library)
- `libconfig++-dev` (Configuration file parser)
- `libmeshtastic` submodule initialized

### Compile

```bash
# Clone repository with submodules
git clone --recursive https://github.com/samurai0000000/meshpump.git
cd meshpump

# Build via Makefile
make

# Or build via CMake
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

The compiled binary `meshpump` will be generated in `build/`.

---

## Quick Start & Shell Usage

Run `meshpump` specifying the serial interface connected to your Meshtastic LoRa radio and optional remote shell port:

```bash
# Start meshpump attached to serial port
./build/meshpump -d /dev/ttyAMA0 -p 4444
```

### Interactive Shell Commands

```text
MeshPump> help
Available commands:
  status               - Display runtime status, relay states, and robot channel
  pump [fish|up] [on|off] [cutoff] - Control water pump relays
  led [cmd|row] [text] - Configure or display text on MAX7219 LED matrix
  lighting [on|off]    - Query or toggle auxiliary night lighting
  env                  - Print CPU temperature and environmental telemetry
  nodes                - List discovered mesh nodes
  wcfg                 - Display Meshtastic radio configuration
  nvm                  - Inspect or commit NVM configuration
```

---

## HomeMesh Automation Integration

`meshpump` automatically announces itself on the Meshtastic robot channel and registers with [MeshMon](https://github.com) gateways. When `MeshMon` issues a targeted `!nodeid identify` (or `all identify`) discovery request, `meshpump` reports its active capabilities:

```text
identify: app=meshpump ver=2.1.4 hw=linux caps=pump_fish,pump_up,led,env
```

`MeshMon` automatically provisions corresponding Home Assistant MQTT Auto-Discovery entities:
- **`switch.meshmon_<node>_pump_fish`**: Fish Pond Pump Power
- **`switch.meshmon_<node>_pump_up`**: Upper Water Pump Power
- **`number.meshmon_<node>_pump_up_cutoff`**: Upper Pump Cutoff Timer (5–120s)
- **`text.meshmon_<node>_led_msg`** / **`button.meshmon_<node>_led_msg`**: LED Matrix Message Display
- **`number.meshmon_<node>_led_delay`**: LED Matrix Scroll Refresh Delay (10–500ms)
- **`sensor.meshmon_<node>_cpu_temp`**: Raspberry Pi CPU Temperature (°C)
- **`sensor.meshmon_<node>_uptime`**: Node Uptime (seconds)
- **`sensor.meshmon_<node>_rtt`**: LoRa Network Round-Trip Response Latency (ms)

---

## License & Copyright

Copyright (C) 2025–2026, Charles Chiou. All rights reserved.
