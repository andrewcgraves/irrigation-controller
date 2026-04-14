# Irrigation Controller — Claude Context

## Project Overview
ESP32-based 4-zone residential irrigation controller running on an **Arduino Nano ESP32** (ESP32-S3 / ABX00083). Controls solenoid valves via 4 relay channels, provides a local UI (rotary encoder + SSD1306 OLED), and integrates with **Home Assistant** over MQTT.

## Build & Flash Commands
```bash
pio run -e nano_esp32                    # compile for hardware
pio run -e nano_esp32 -t upload          # compile + flash via USB
pio run -e ota -t upload                 # flash over-the-air (irrigation-controller.local)
pio test -e native                       # run all 105 desktop unit tests (no hardware needed)
```

Build environments are defined in `platformio.ini`.

## Architecture

Single-threaded Arduino loop. No RTOS, no heap allocation, no threads.

```
main.cpp
└── StateMachine              — orchestration; owns state transitions
    ├── ZoneController        — relay sequencing, queue-based watering
    ├── Scheduler             — 8 schedule slots, NVS persistence
    └── MenuSystem            — encoder-driven UI, menu tree
MQTTManager                   — WiFi + MQTT + Home Assistant discovery
InputManager                  — rotary encoder quadrature + button debounce
DisplayManager                — SSD1306 128×32 SPI OLED rendering
LEDManager                    — per-zone status LEDs (blink/solid/off)
ClockManager                  — NTP sync, time queries
ESP32HAL                      — hardware abstraction (wraps GPIO, NVS, OTA)
```

All hardware-touching code implements abstract interfaces (`IHAL`, `IClock`, `IInputManager`) so tests can run on desktop with mocks.

**StateMachine uses a callback interface** (`IStateMachineCallbacks`) to notify Display and MQTT — it does not own them directly.

### Key Source Files
| File | Purpose |
|---|---|
| `src/main.cpp` | Setup/loop, module wiring |
| `src/state/StateMachine.cpp` | State machine (Init→Idle→MenuNav→ManualSetup→Watering→Complete) |
| `src/zone/ZoneController.cpp` | Active-LOW relay control, zone queue |
| `src/scheduler/Scheduler.cpp` | Schedule matching, NVS read/write (16 bytes/slot) |
| `src/network/MQTTManager.cpp` | WiFi reconnect, MQTT exponential backoff, HA discovery |
| `src/ui/MenuSystem.cpp` | Full menu tree navigation |
| `src/display/DisplayManager.cpp` | SSD1306 screen rendering |
| `src/input/InputManager.cpp` | Encoder + button events |
| `src/led/LEDManager.cpp` | Per-zone LED blink logic |
| `src/clock/ClockManager.cpp` | NTP, timezone (UTC-7, hardcoded) |
| `include/config.h` | All pin assignments and timing constants |
| `include/secrets.h` | WiFi SSID/password, MQTT broker (gitignored — create manually) |

## Key Configuration (`include/config.h`)
```cpp
NUM_ZONES = 4
MAX_WATERING_DURATION_MIN = 30
DEFAULT_WATERING_DURATION_MIN = 10
MQTT_PORT = 1883
MQTT_RECONNECT_MIN_MS = 1000
MQTT_RECONNECT_MAX_MS = 60000
WIFI_CONNECT_TIMEOUT_MS = 20000
NTP_SYNC_INTERVAL_MS = 3600000   // 1 hour
DISPLAY_IDLE_TIMEOUT_MS = 60000  // display off after 60s
MENU_IDLE_TIMEOUT_MS = 30000     // return to home after 30s
LED_BLINK_INTERVAL_MS = 500
LONG_PRESS_MS = 1000
SCHEDULER_MAX_SLOTS = 8
SCHEDULER_MAX_ZONES_PER_SLOT = 4
```

`include/secrets.h` is gitignored and must be created manually:
```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
#define MQTT_SERVER "192.168.x.x"
#define MQTT_USER "user"
#define MQTT_PASSWORD "password"
```

## MQTT Protocol

### Command Topics (subscribe)
| Topic | Payload | Action |
|---|---|---|
| `sprinkler/cmd/on` | `{"zones":[{"zone":1,"duration":10},{"zone":2,"duration":15}]}` | Start watering |
| `sprinkler/cmd/off` | _(any)_ | Stop all zones |
| `sprinkler/cmd/status` | _(any)_ | Publish current status |
| `sprinkler/cmd/schedule/set` | JSON schedule object | Save schedule slot |
| `sprinkler/cmd/schedule/delete` | `{"slot":0}` | Delete schedule slot |
| `sprinkler/on/{zones}` | `{duration}` | Legacy: comma-separated zone IDs |

### Status Topics (publish)
| Topic | Payload | Notes |
|---|---|---|
| `sprinkler/status/online` | `"online"` / `"offline"` | Retained, LWT |
| `sprinkler/status/watering` | `{"zone":1,"remaining":540}` or `"off"` | Active zone |
| `sprinkler/status/zone/{n}` | `"on"` / `"off"` | Retained, per-zone |

### Home Assistant Discovery
Auto-publishes to `homeassistant/switch/irrigation_zone{n}/config` on connect. Device name: "Irrigation Controller", model: "Arduino Nano ESP32".

## Hardware

### Pin Assignments
| Signal | GPIO | Arduino Label |
|---|---|---|
| Zone 1 relay | 5 | D2 |
| Zone 2 relay | 15 | D4 |
| Zone 3 relay | 16 | D5 |
| Zone 4 relay | 17 | D6 |
| OLED MOSI/SDA | 38 | D11 |
| OLED CLK/SCL | 39 | D12 |
| OLED CS | 40 | D13 |
| OLED DC | 1 | A0 |
| OLED RST | 2 | A1 |
| Encoder CLK (A) | 3 | A2 |
| Encoder DT (B) | 4 | A3 |
| Encoder SW | 13 | A6 |
| Zone 1 LED | 14 | A7 |
| Zone 2 LED | 44 | D0 |
| Zone 3 LED | 43 | D1 |
| Zone 4 LED | 18 | D7 |

**Relay logic**: Active-LOW — `LOW` = valve ON, `HIGH` = valve OFF.

See `docs/WIRING.md` for the full wiring diagram and BOM.

## Testing

105 desktop unit tests in `test/test_native/`, organized into 5 suites:
- `test_zone_controller` — queue management, relay sequencing, timing
- `test_mqtt_parsing` — JSON and legacy command parsing
- `test_scheduler` — day matching, schedule firing, NVS serialization
- `test_menu_system` — menu navigation, schedule building via UI
- `test_state_machine` — state transitions, priority rules, callback invocations

Mock objects in `test/mocks/`: `MockHAL`, `MockClock`.

Run with: `pio test -e native`

## Coding Conventions
- **No dynamic allocation**: All data structures use fixed-size arrays (e.g., `ZoneRunQueue` uses a plain array, `Schedule` is a plain struct). No `new`/`malloc`.
- **Abstract interfaces for testability**: Any class touching hardware implements an interface (`IHAL`, `IClock`, `IInputManager`).
- **Non-blocking operations**: WiFi connection, MQTT reconnect, and NTP sync are all non-blocking with timeouts managed in `loop()`.
- **Exponential backoff**: MQTT reconnect doubles interval on failure (1s → 60s max), resets on success.
- **NVS serialization**: Schedules stored as 16 bytes/slot under namespace `"irrigation"`. See `Scheduler.cpp` for the binary layout.
- **Watchdog**: 30-second WDT fed every `loop()` iteration via `esp_task_wdt_reset()`.
