# MeshPump HomeChat Protocol Extensions

`meshpump` extends the base `HomeChat` protocol on Linux and Raspberry Pi platforms to provide decentralized control over water pump relays (fish pond circulation and upper elevation replenishment with safety cut-off protection), multi-row MAX7219 LED dot-matrix scrolling displays, CPU thermal diagnostics, and automated registration within the HomeMesh ecosystem.

---

## 1. Water Pump Control (`pump`)

Controls independent water pump relays:

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `pump fish on` / `pump 0 on` | Turn Fish Pond circulation pump relay on. | `pump: fish=on` |
| `pump fish off` / `pump 0 off` | Turn Fish Pond circulation pump relay off. | `pump: fish=off` |
| `pump up on [cutoff_sec]` / `pump 1 on [cutoff_sec]` | Turn Upper pump relay on with auto-cutoff safety timer (default configured or 5..120 sec). | `pump: up=on cutoff=30s` |
| `pump up off` / `pump 1 off` | Turn Upper pump relay off. | `pump: up=off` |
| `pump` / `pump status` | Query current status of all pump relays and safety timers. | `pump: fish=on up=off cutoff=30s` |

---

## 2. LED Matrix Display Control (`led`)

Controls attached MAX7219 8x8 / 8x32 cascading LED dot-matrix display panels:

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `led <row> <text>` | Display or scroll text on specified matrix row (row `0`..`3`). | `Led matrix updated for NodeName` |
| `led delay <ms>` | Set global scrolling step refresh interval (milliseconds). | `set delay to 50ms` |
| `led sf <row> <factor>` | Set per-row slowdown factor multiplier (1..N). | `set sf of row 0 to 2` |
| `led blank` | Clear / blank all LED matrix rows. | `Led matrix updated for NodeName` |
| `led welcome` | Reset LED matrix to default scrolling welcome message. | `Led matrix updated for NodeName` |
| `led` | Query current display refresh delay, row TTL, and slowdown factors. | `delay: 50ms\nrow 0: ttl=0s, sf=1...` |

---

## 3. System & Diagnostics Extensions

- **`status`**:
  - Reports operational status of pump relays and safety cut-off configuration:
    ```text
    status: fish=on up=off up_cutoff=30s
    ```
- **`env`**:
  - In addition to standard ambient sensors (moisture, temperature, humidity), appends Raspberry Pi Broadcom VCIO CPU core temperature:
    ```text
    env: temp_cpu=42.5
    ```

---

## 4. HomeMesh Auto-Discovery & Rollcall

`meshpump` supports machine-readable capability reporting for automatic registration with `meshmon` gateways and Home Assistant:

* **Command**: `rollcall [target]`
  * If `target` is specified (by hex node ID, decimal ID, short name, or long name), `meshpump` only responds if targeted to itself or `"all"`.
* **Structured Response**:
  ```text
  rollcall: app=meshpump ver=2.1.3 hw=linux caps=pump_fish,pump_up,led,env
  ```

### Exported Subsystem Capabilities:
- `pump_fish`: Fish pond water circulation pump switch (`switch.meshmon_<node>_pump_fish`).
- `pump_up`: Upper water pump switch with auto-cutoff protection (`switch.meshmon_<node>_pump_up`, `number.meshmon_<node>_pump_up_cutoff`).
- `led`: MAX7219 LED dot-matrix text display (`text.meshmon_<node>_led_msg`, `number.meshmon_<node>_led_delay`).
- `env`: Broadcom VCIO CPU core thermals and environmental telemetry (`sensor.meshmon_<node>_cpu_temp`).
