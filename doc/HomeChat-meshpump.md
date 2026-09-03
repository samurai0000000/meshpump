# MeshPump HomeChat Protocol Extensions

`meshpump` extends the `HomeChat` protocol on Raspberry Pi / Linux nodes for water pump relay control, automated watering schedules, and MAX7219 8x8/8x32 LED matrix message displays.

---

## 1. Water Pump Control (`pump`)

Controls water pump relays and pulse timers:

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `pump on` | Manually activate pump relay. | `pump: state=on` |
| `pump off` | Turn pump relay off. | `pump: state=off` |
| `pump pulse <duration_ms>` | Run pump for specified duration (e.g. 5000 ms) and auto-shutoff. | `pump: state=pulsing dur=5000ms` |
| `pump auto [on\|off]` | Enable or disable automated sensor-driven watering schedule. | `pump: auto=on` |
| `pump status` / `pump` | Query current pump state, schedule, and safety cut-off timer. | `pump: state=off auto=on last_run=1200s_ago` |

---

## 2. LED Matrix Display Control (`led`)

Controls attached MAX7219 LED dot-matrix panels:

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `led msg <text>` | Scroll or display text message across the LED matrix. | `led: msg='HELLO'` |
| `led on` | Turn on the display. | `led: state=on` |
| `led off` | Clear and put display into sleep mode. | `led: state=off` |
| `led brightness <0-15>` | Set LED matrix intensity (0 to 15). | `led: brightness=8` |
| `led` | Query current display state and brightness. | `led: state=on brightness=8` |

---

## 3. System & Diagnostics Extensions

- **`env`**:
  - Reports environmental soil moisture, water reservoir levels, ambient temperature, and humidity:
    ```text
    env: temp=24.1 hum=52.0 moisture=68% reservoir=ok
    ```
- **`status`**:
  - Summarizes operational health, relay interlock states, and watchdog status.
