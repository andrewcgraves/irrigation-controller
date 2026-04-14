# Irrigation Controller

A modular, testable irrigation controller for the Arduino Nano ESP32 (ESP32-S3). Supports both online operation via Home Assistant/MQTT and fully offline scheduling with an OLED display and rotary encoder.

## Features

- **4-zone sprinkler control** with per-zone durations (1-30 min)
- **Home Assistant integration** via MQTT with auto-discovery
- **Offline scheduling** — 8 schedule slots, configurable days/time/zones via encoder
- **Manual run** — select zones and durations from the on-device menu
- **OLED display** (SSD1306 128x32 SPI) with idle timeout for burn-in protection
- **Per-zone LEDs** — blink = active, solid = completed, off = idle
- **OTA updates** — flash new firmware over WiFi
- **Watchdog timer** — automatic recovery from crashes
- **105 unit tests** — all business logic tested on desktop without hardware

## Architecture

```
main.cpp (wiring)
    |
    +-- StateMachine (orchestration)
    |       |-- ZoneController (relay control, single-zone-at-a-time)
    |       |-- Scheduler (8 slots, NVS persistence, day/time matching)
    |       |-- MenuSystem (screen navigation, input handling)
    |       +-- IStateMachineCallbacks --> Display / MQTT / NVS
    |
    +-- MQTTManager (WiFi, MQTT, HA Discovery)
    +-- InputManager (PEC11 encoder + button)
    +-- DisplayManager (SSD1306 OLED rendering)
    +-- LEDManager (per-zone status LEDs)
    +-- ClockManager (NTP time sync)
    +-- HAL (hardware abstraction for testability)
```

All modules depend on abstract interfaces (`IHAL`, `IClock`, `IInputManager`) so they can be tested on desktop with mocks.

## Flashing the Microcontroller

### 1. Install PlatformIO

PlatformIO handles downloading the ESP32 toolchain, all libraries, and the flash tool automatically — you do not need to install the Arduino IDE.

**Option A — VS Code extension (recommended for beginners):**
1. Install [VS Code](https://code.visualstudio.com/)
2. Open the Extensions panel, search for **PlatformIO IDE**, install it
3. Open this project folder in VS Code — PlatformIO detects `platformio.ini` automatically

**Option B — Command line:**
```bash
# macOS / Linux
pip install platformio

# Verify
pio --version
```

### 2. Install the Arduino Nano ESP32 Board Driver

The Nano ESP32 uses a USB-C port that presents as a serial port. On macOS and Linux it works without a driver. On Windows, install the [USB CDC driver from Arduino](https://www.arduino.cc/en/Guide/ArduinoNanoESP32).

To confirm the board is detected (plug in via USB-C first):
```bash
pio device list
# Should show something like: /dev/cu.usbmodem... (macOS) or COM3 (Windows)
```

### 3. Configure Credentials

```bash
cp include/secrets.h.example include/secrets.h
```

Edit `include/secrets.h` and fill in:
- Your WiFi SSID and password
- Your MQTT broker IP address and credentials

### 4. First Flash (USB)

The very first upload must be done over USB. PlatformIO will compile all source files and libraries (~30 seconds the first time, cached after that) and then flash the firmware.

```bash
# Build only — verify it compiles cleanly without uploading
pio run -e nano_esp32

# Build and upload over USB
pio run -e nano_esp32 -t upload
```

**If the upload fails with "No serial port found":**
- Check the USB-C cable is a data cable (not charge-only)
- Try pressing the RESET button on the Nano ESP32 just before the upload starts
- On the Nano ESP32, hold the BOOT button while plugging in USB to force bootloader mode

**If the upload fails with permission denied on macOS/Linux:**
```bash
sudo usermod -aG dialout $USER   # Linux
# macOS: no action needed, try unplugging and replugging
```

### 5. Verify It's Running

Open the serial monitor immediately after flashing to see the boot log:

```bash
pio device monitor
# (115200 baud, set in platformio.ini)
```

Expected output:
```
Irrigation Controller starting...
WiFi connecting...
WiFi connected
MQTT connecting...connected
HA MQTT Discovery published
Setup complete.
```

The OLED should display the home screen with the current time once NTP syncs (requires WiFi).

### 6. Subsequent Flashes (OTA over WiFi)

Once the controller is mounted in its enclosure and WiFi is working, you can flash new firmware without touching the USB cable:

```bash
pio run -e ota -t upload
```

This uses mDNS to find the board at `irrigation-controller.local` on your network. The OLED will show "OTA Update / Flashing..." during the process and reboot automatically when done.

**If OTA upload cannot find the board:**
- Confirm the board is online: `ping irrigation-controller.local`
- OTA requires the board to already be running firmware that includes `ArduinoOTA.handle()` in the loop (all builds from this project include it)
- Check that your computer and the board are on the same WiFi network/VLAN

### 7. Run Tests Without Hardware

All business logic can be tested on your development machine — no board required:

```bash
pio test -e native
# Expected: 105 test cases: 105 succeeded
```

### MQTT Topics

**Inbound (Home Assistant -> Controller):**

| Topic | Payload | Action |
|-------|---------|--------|
| `sprinkler/cmd/on` | `{"zones":[{"zone":1,"duration":10}]}` | Start watering |
| `sprinkler/cmd/off` | (any) | Stop all zones |
| `sprinkler/cmd/status` | (any) | Request status report |
| `sprinkler/on/1,2,3` | `10` | Legacy format (duration in minutes) |

**Outbound (Controller -> Home Assistant):**

| Topic | Payload | Retained |
|-------|---------|----------|
| `sprinkler/status/online` | `online` / `offline` (LWT) | Yes |
| `sprinkler/status/watering` | `{"zone":1,"remaining":540}` or `off` | No |
| `sprinkler/status/zone/{n}` | `on` / `off` | Yes |

### Priority Rules

1. **MQTT commands always win** — can start/stop watering from any state
2. **Scheduled watering** only starts if nothing is currently running
3. **Button press during watering** = emergency stop
4. **Offline scheduling** continues working when WiFi is down

## Testing

```bash
# Run all 105 tests
pio test -e native

# Run a specific test suite
pio test -e native -f test_native/test_zone_controller
pio test -e native -f test_native/test_scheduler
pio test -e native -f test_native/test_mqtt_parsing
pio test -e native -f test_native/test_menu_system
pio test -e native -f test_native/test_state_machine
```

## Menu Navigation

The rotary encoder provides all navigation:

| Action | Input |
|--------|-------|
| Scroll / adjust value | Rotate CW/CCW |
| Select / confirm / toggle | Short press |
| Back / cancel | Long press (>1 sec) |

**Menu tree:**

```
Home Screen (time, WiFi status)
  +-- Manual Run
  |     +-- Zone Select (toggle zones with press, long press to confirm)
  |     +-- Duration per zone (rotate to adjust, press for next zone)
  |     +-- Confirm (Yes/No)
  +-- Schedules
  |     +-- Slot 1-8
  |           +-- Edit -> Days -> Time -> Zones -> Duration -> Confirm
  |           +-- On/Off (toggle)
  |           +-- Delete
  +-- Settings
  +-- Status (uptime)
```

## Hardware

See [docs/WIRING.md](WIRING.md) for complete wiring diagram, pin assignments, and assembly checklist.

## Project Structure

```
irrigation-controller/
+-- platformio.ini          # Build environments (nano_esp32, native, ota)
+-- include/
|   +-- config.h            # Pin assignments, constants
|   +-- secrets.h           # WiFi/MQTT credentials (gitignored)
+-- src/
|   +-- main.cpp            # setup() + loop(), wires all modules
|   +-- hal/                # Hardware abstraction layer
|   +-- zone/               # Relay control, run queue
|   +-- scheduler/          # Schedule storage, NVS persistence
|   +-- network/            # WiFi, MQTT, HA Discovery
|   +-- display/            # SSD1306 OLED rendering
|   +-- input/              # Rotary encoder + button
|   +-- ui/                 # Menu system, screen navigation
|   +-- state/              # State machine orchestration
|   +-- clock/              # NTP time sync
|   +-- led/                # Per-zone LED control
+-- test/
|   +-- test_native/        # 105 desktop unit tests
|   +-- mocks/              # MockHAL, MockClock
+-- docs/
    +-- WIRING.md           # Wiring diagram & assembly guide
    +-- README.md           # This file
```
