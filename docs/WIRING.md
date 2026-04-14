# Irrigation Controller Wiring Guide

## Bill of Materials

| Component | Spec | Qty | Purpose |
|-----------|------|-----|---------|
| Arduino Nano ESP32 | ABX00083 (ESP32-S3) | 1 | Main controller |
| OLED Display | SSD1306 128x32 SPI | 1 | Menus & status |
| Rotary Encoder | PEC11 Series 12mm incremental (with push button) | 1 | Navigation + confirm |
| Per-Zone LEDs | 5mm green | 4 | Zone status indicators |
| LED Resistors | 220 ohm 1/4W | 4 | Current limiting |
| Relay Module | 4-ch 5V active-LOW, optocoupler-isolated | 1 | Solenoid valve control |
| Power Supply | 5V 2A USB or barrel jack | 1 | Logic power |
| Solenoid PSU | 24VAC transformer (existing) | 1 | Valve power |

---

## Pin Assignment Table

### Relay Outputs (Active LOW: LOW = ON, HIGH = OFF)

| Signal | Nano ESP32 Pin | GPIO | Relay Channel | Wire Color (suggested) |
|--------|---------------|------|---------------|----------------------|
| Zone 1 Relay | D2 | GPIO 5 | CH1 | Red |
| Zone 2 Relay | D4 | GPIO 15 | CH2 | Orange |
| Zone 3 Relay | D5 | GPIO 16 | CH3 | Yellow |
| Zone 4 Relay | D6 | GPIO 17 | CH4 | Green |

### SPI OLED Display (SSD1306 128x32)

| Signal | Nano ESP32 Pin | Arduino Pin | OLED Pin | Wire Color (suggested) |
|--------|---------------|-------------|----------|----------------------|
| MOSI (Data) | D9 | 9 | SDA/MOSI | Blue |
| CLK (Clock) | D10 | 10 | SCL/CLK | Purple |
| DC (Data/Command) | D11 | 11 | DC | White |
| CS (Chip Select) | D12 | 12 | CS | Gray |
| RST (Reset) | D8 | 8 | RST | Brown |
| VCC | 3V3 | - | VCC | Red |
| 3.3VO | — | - | 3.3VO | Leave unconnected (regulator output) |
| GND | GND | - | GND | Black |

### Rotary Encoder (PEC11 12mm)

| Signal | Nano ESP32 Pin | GPIO | Encoder Pin | Note |
|--------|---------------|------|-------------|------|
| CLK (Phase A) | A2 | GPIO 3 | A | Internal pullup enabled |
| DT (Phase B) | A3 | GPIO 4 | B | Internal pullup enabled |
| SW (Button) | A6 | GPIO 13 | SW | Internal pullup enabled |
| GND | GND | - | C (common) | Shared ground |

The PEC11 encoder has 5 pins: A, B, C (common), and two for the push button (SW + GND).
The internal pullups on the ESP32 are used, so no external resistors are needed.

### Per-Zone LEDs

| Signal | Nano ESP32 Pin | GPIO | LED | Resistor |
|--------|---------------|------|-----|----------|
| Zone 1 LED | A7 | GPIO 14 | LED 1 Anode | 220 ohm to GPIO |
| Zone 2 LED | D0 | GPIO 44 | LED 2 Anode | 220 ohm to GPIO |
| Zone 3 LED | D1 | GPIO 43 | LED 3 Anode | 220 ohm to GPIO |
| Zone 4 LED | D7 | GPIO 18 | LED 4 Anode | 220 ohm to GPIO |

All LED cathodes connect to GND. Each LED has a 220 ohm resistor in series.

LED Behavior:
- **Blinking** (500ms interval) = Zone is actively watering
- **Solid ON** = Zone completed its watering cycle
- **OFF** = Zone is idle / inactive

### Power

| Signal | Source | Destination | Note |
|--------|--------|-------------|------|
| 5V | USB / barrel PSU | Nano ESP32 VIN | Powers MCU |
| 5V | Nano ESP32 5V | Relay module VCC | Powers relay coils |
| GND | Common | All GND pins | Shared ground bus |
| 3.3V | Nano ESP32 3V3 | OLED VCC | Display power |
| 24VAC | Transformer | Relay COM terminals | Solenoid valve power |

---

## Wiring Diagram (ASCII)

```
                        Arduino Nano ESP32
                     +----------------------+
                     |                      |
              USB -->| USB-C          3V3 o-+---------> OLED VCC
                     |                      |
                     | D2  (GPIO  5) o------+---------> Relay CH1 IN (Zone 1)
                     | D4  (GPIO 15) o------+---------> Relay CH2 IN (Zone 2)
                     | D5  (GPIO 16) o------+---------> Relay CH3 IN (Zone 3)
                     | D6  (GPIO 17) o------+---------> Relay CH4 IN (Zone 4)
                     |                      |
                     | D8  (pin   8) o------+---------> OLED RST
                     | D9  (pin   9) o------+---------> OLED SDA (MOSI)
                     | D10 (pin  10) o------+---------> OLED SCL (CLK)
                     | D11 (pin  11) o------+---------> OLED DC
                     | D12 (pin  12) o------+---------> OLED CS
                     |                      |
                     | A2  (GPIO  3) o------+---------> Encoder CLK (A)
                     | A3  (GPIO  4) o------+---------> Encoder DT  (B)
                     | A6  (GPIO 13) o------+---------> Encoder SW
                     |                      |
                     | A7  (GPIO 14) o--[220R]--+-----> LED 1 (+) Zone 1
                     | D0  (GPIO 44) o--[220R]--+-----> LED 2 (+) Zone 2
                     | D1  (GPIO 43) o--[220R]--+-----> LED 3 (+) Zone 3
                     | D7  (GPIO 18) o--[220R]--+-----> LED 4 (+) Zone 4
                     |                      |
                     | 5V             o-----+---------> Relay Module VCC
                     | GND            o--+--+---------> Relay Module GND
                     +-------------------+--+
                                         |
                     GND Bus ============+======+======+======+======+
                                         |      |      |      |      |
                                     OLED GND   |   Enc GND   |      |
                                              LED1(-) LED2(-) LED3(-) LED4(-)


         4-Channel Relay Module                    24VAC Solenoid Valves
    +----------------------------+            +---------------------------+
    |                            |            |                           |
    | IN1 <-- Zone 1 (GPIO  5)  |            |  Valve 1 <-- Relay 1 NO  |
    | IN2 <-- Zone 2 (GPIO 15)  |  24VAC     |  Valve 2 <-- Relay 2 NO  |
    | IN3 <-- Zone 3 (GPIO 16)  |  Common -->|  Valve 3 <-- Relay 3 NO  |
    | IN4 <-- Zone 4 (GPIO 17)  |  Wire      |  Valve 4 <-- Relay 4 NO  |
    |                            |            |                           |
    | VCC <-- 5V                 |            |  Common wire to 24VAC    |
    | GND <-- GND                |            |  transformer return      |
    +----------------------------+            +---------------------------+

         Relay Wiring Detail (per channel):

         24VAC Hot ----> Relay COM
                         Relay NO -----> Solenoid Valve
         24VAC Return (common) -------> All Valve Commons (daisy-chained)
```

---

## Relay Wiring Detail

Each relay channel switches 24VAC power to a solenoid valve:

```
    24VAC Transformer
    +-----------+
    | HOT    o--+---> Relay 1 COM ---- NO ---> Valve 1 --|
    |           +---> Relay 2 COM ---- NO ---> Valve 2 --|
    |           +---> Relay 3 COM ---- NO ---> Valve 3 --|
    |           +---> Relay 4 COM ---- NO ---> Valve 4 --|
    |           |                                         |
    | COMMON o--+-----------------------------------------+
    +-----------+       (all valve commons daisy-chained)
```

- **COM** = Common terminal on relay
- **NO** = Normally Open (closed when relay energized = GPIO LOW)
- **NC** = Normally Closed (not used)
- The 24VAC hot wire connects to all relay COM terminals
- Each relay NO connects to its solenoid valve
- All solenoid valve common wires connect back to the 24VAC transformer return

**IMPORTANT: The relay module is active-LOW.** GPIO LOW = relay ON = valve open. GPIO HIGH = relay OFF = valve closed. The firmware handles this automatically.

---

## PEC11 Encoder Wiring Detail

```
    PEC11 Encoder (bottom view, pins facing you)

         A     C     B
         |     |     |
         |     |     |
    GPIO 3   GND   GPIO 4
    (CLK)          (DT)

    Push button pins (side):
         SW ---- GPIO 13
         GND --- GND bus
```

The PEC11 has a built-in push-button switch. The two side pins are the switch contacts. One goes to GPIO 13 (A6), the other to GND. The ESP32 internal pullup resistor pulls the pin HIGH when the button is not pressed. Pressing the button connects it to GND (reads LOW).

Rotation is detected by reading the quadrature signals on pins A and B:
- Clockwise rotation: A leads B
- Counter-clockwise rotation: B leads A

---

## LED Wiring Detail

```
    GPIO pin --[220 ohm]--+--|>|-- GND
                          resistor  LED
                                  (anode to resistor, cathode to GND)
```

Each LED uses a 220 ohm current-limiting resistor. At 3.3V with a ~2V green LED forward voltage:
- Current = (3.3V - 2.0V) / 220 ohm = ~6mA per LED (well within ESP32 GPIO limits of 40mA)

---

## SPI OLED Wiring Detail

```
    OLED Module (SSD1306 128x32 SPI)
    +--------+
    | GND    |--- GND bus
    | VCC    |--- 3.3V (from Nano ESP32 3V3 pin)
    | 3.3VO  |--- leave unconnected (regulator output, not needed here)
    | SCL    |--- D10 (pin 10) - SPI Clock
    | SDA    |--- D9  (pin  9) - SPI MOSI (Data)
    | RST    |--- D8  (pin  8) - Reset
    | DC     |--- D11 (pin 11) - Data/Command select
    | CS     |--- D12 (pin 12) - Chip Select
    +--------+
```

Note: Some SSD1306 SPI modules label MOSI as "SDA" or "DIN" — this is the data line, not I2C SDA.

The Nano ESP32 only exposes 3.3V as a power output — there is no always-on 5V pin. Connect VCC directly to 3V3; the SSD1306 runs natively at 3.3V so no regulator is needed. The 3.3VO pin can be left unconnected, or used to tap 3.3V for another low-current device.

---

## GPIO Usage Summary

| Arduino Pin | Nano Pin | Function | Direction | Notes |
|-------------|----------|----------|-----------|-------|
| 3 | A2 | Encoder CLK | Input | Internal pullup |
| 4 | A3 | Encoder DT | Input | Internal pullup |
| 5 | D2 | Relay Zone 1 | Output | Active LOW |
| 8 | D8 | OLED RST | Output | Display reset |
| 9 | D9 | OLED MOSI | Output | Software SPI data |
| 10 | D10 | OLED CLK | Output | Software SPI clock |
| 11 | D11 | OLED DC | Output | Data/Command select |
| 12 | D12 | OLED CS | Output | Chip Select |
| 13 | A6 | Encoder SW | Input | Internal pullup |
| 14 | A7 | LED Zone 1 | Output | Via 220 ohm |
| 15 | D4 | Relay Zone 2 | Output | Active LOW |
| 16 | D5 | Relay Zone 3 | Output | Active LOW |
| 17 | D6 | Relay Zone 4 | Output | Active LOW |
| 18 | D7 | LED Zone 4 | Output | Via 220 ohm |
| 43 | D1 | LED Zone 3 | Output | Via 220 ohm |
| 44 | D0 | LED Zone 2 | Output | Via 220 ohm |

**Total: 16 pins used**

### Restricted GPIOs (DO NOT USE)

| GPIO | Reason |
|------|--------|
| 6-11 | Connected to SPI flash on the Nano ESP32 module |
| 0 (B0) | Strapping pin - safe as output after boot, avoid for inputs |
| 46 (B1) | Strapping pin - safe as output after boot, avoid for inputs |

---

## Power Notes

1. **5V Power**: The Nano ESP32 can be powered via USB-C or the VIN pin. The 5V pin provides regulated 5V output to power the relay module.

2. **3.3V Rail**: The Nano ESP32's 3.3V regulator powers the OLED display. Total 3.3V current draw: ~25mA (OLED) + ~6mA per LED x 4 = ~49mA max. Well within the regulator's capability.

3. **Relay Module**: The optocoupler-isolated relay module draws ~70mA per active channel from the 5V rail. Since only one zone runs at a time, max draw is ~70mA.

4. **24VAC**: The solenoid valves are powered by a separate 24VAC transformer. Keep 24VAC wiring well separated from the low-voltage logic wiring.

**WARNING**: Never connect 24VAC wiring to any ESP32 pin. The relay module provides galvanic isolation between the logic side and the valve side.

---

## Assembly Checklist

- [ ] Solder header pins to Arduino Nano ESP32 (if not pre-soldered)
- [ ] Wire relay module: VCC to 5V, GND to GND, IN1-IN4 to GPIO 5/15/16/17
- [ ] Wire OLED display: VCC to 3V3, GND to GND, SPI data/clock/CS/DC/RST
- [ ] Wire PEC11 encoder: A to GPIO 3, B to GPIO 4, C to GND, SW to GPIO 13, SW-GND to GND
- [ ] Wire LEDs with 220 ohm resistors: anodes to GPIO 14/44/43/18, cathodes to GND
- [ ] Wire 24VAC transformer hot to all relay COM terminals
- [ ] Wire each relay NO to its corresponding solenoid valve
- [ ] Daisy-chain all solenoid valve common wires to 24VAC transformer return
- [ ] Double-check all connections before powering on
- [ ] Power on via USB, verify serial output: "Irrigation Controller starting..."
- [ ] Verify OLED displays home screen
- [ ] Test encoder rotation and button press navigate menus
- [ ] Test MQTT connectivity from Home Assistant
- [ ] Run a 1-minute manual test on each zone, verify relay clicks and LED blinks
