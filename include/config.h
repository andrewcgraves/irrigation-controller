#pragma once

// ============================================
// Irrigation Controller Configuration
// Target: Arduino Nano ESP32 (ESP32-S3)
// ============================================

// --- Zone Configuration ---
static const int NUM_ZONES = 4;
static const int MAX_WATERING_DURATION_MIN = 30;
static const int DEFAULT_WATERING_DURATION_MIN = 10;

// --- Relay Pins (active LOW) ---
// NOTE: Board uses BOARD_HAS_PIN_REMAP. These are Arduino API pin numbers, NOT GPIO numbers.
// Arduino pin 2 = D2 = GPIO 5, pin 4 = D4 = GPIO 7, pin 5 = D5 = GPIO 8, pin 6 = D6 = GPIO 9
static const int PIN_RELAY_ZONE1 = 2;   // D2 (GPIO 5)
static const int PIN_RELAY_ZONE2 = 4;   // D4 (GPIO 7)
static const int PIN_RELAY_ZONE3 = 5;   // D5 (GPIO 8)
static const int PIN_RELAY_ZONE4 = 6;   // D6 (GPIO 9)

static const int ZONE_RELAY_PINS[NUM_ZONES] = {
    PIN_RELAY_ZONE1,
    PIN_RELAY_ZONE2,
    PIN_RELAY_ZONE3,
    PIN_RELAY_ZONE4
};

// Relay logic: HIGH = ON, LOW = OFF
// Active-HIGH: GPIO defaults LOW on boot, so relays stay off until explicitly activated.
static const int RELAY_ON  = 1;
static const int RELAY_OFF = 0;

// --- SPI OLED (SSD1306 128x32) ---
static const int PIN_OLED_MOSI = 9;
static const int PIN_OLED_CLK  = 10;
static const int PIN_OLED_DC   = 11;
static const int PIN_OLED_CS   = 12;
static const int PIN_OLED_RST  = 8;   // D8

static const int OLED_WIDTH  = 128;
static const int OLED_HEIGHT = 32;

// --- Rotary Encoder (PEC11) ---
// Arduino Nano ESP32 pin numbers: A2=19, A3=20, A6=23, A7=24
static const int PIN_ENCODER_CLK = 19;  // A2
static const int PIN_ENCODER_DT  = 20;  // A3
static const int PIN_ENCODER_SW  = 23;  // A6

// --- Per-Zone LEDs ---
// Arduino API pin numbers (BOARD_HAS_PIN_REMAP active):
// pin 24 = A7 = GPIO 14, pin 0 = D0 = GPIO 44, pin 1 = D1 = GPIO 43, pin 7 = D7 = GPIO 10
static const int PIN_LED_ZONE1 = 24;  // A7 (GPIO 14)
static const int PIN_LED_ZONE2 = 0;   // D0 (GPIO 44)
static const int PIN_LED_ZONE3 = 1;   // D1 (GPIO 43)
static const int PIN_LED_ZONE4 = 7;   // D7 (GPIO 10)

static const int ZONE_LED_PINS[NUM_ZONES] = {
    PIN_LED_ZONE1,
    PIN_LED_ZONE2,
    PIN_LED_ZONE3,
    PIN_LED_ZONE4
};

// --- Timing Constants ---
static const unsigned long DISPLAY_IDLE_TIMEOUT_MS = 60000;   // 60s screen off
static const unsigned long MENU_IDLE_TIMEOUT_MS    = 30000;   // 30s back to home
static const unsigned long LED_BLINK_INTERVAL_MS   = 500;     // 500ms blink rate
static const unsigned long ENCODER_DEBOUNCE_MS     = 5;
static const unsigned long BUTTON_DEBOUNCE_MS      = 50;
static const unsigned long LONG_PRESS_MS           = 1000;    // 1s for back/cancel

// --- Network Constants ---
static const int MQTT_PORT = 1883;
static const unsigned long MQTT_RECONNECT_MIN_MS = 1000;
static const unsigned long MQTT_RECONNECT_MAX_MS = 60000;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
static const unsigned long NTP_SYNC_INTERVAL_MS = 3600000;    // 1 hour

// --- Scheduler Constants ---
static const int MAX_SCHEDULES = 8;
