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
static const int PIN_RELAY_ZONE1 = 5;   // D2
static const int PIN_RELAY_ZONE2 = 15;  // D4
static const int PIN_RELAY_ZONE3 = 16;  // D5
static const int PIN_RELAY_ZONE4 = 17;  // D6

static const int ZONE_RELAY_PINS[NUM_ZONES] = {
    PIN_RELAY_ZONE1,
    PIN_RELAY_ZONE2,
    PIN_RELAY_ZONE3,
    PIN_RELAY_ZONE4
};

// Relay logic: LOW = ON, HIGH = OFF
static const int RELAY_ON  = 0;
static const int RELAY_OFF = 1;

// --- SPI OLED (SSD1306 128x32) ---
static const int PIN_OLED_MOSI = 9;
static const int PIN_OLED_CLK  = 10;
static const int PIN_OLED_DC   = 11;
static const int PIN_OLED_CS   = 12;
static const int PIN_OLED_RST  = 8;   // D8

static const int OLED_WIDTH  = 128;
static const int OLED_HEIGHT = 32;

// --- Rotary Encoder (PEC11) ---
static const int PIN_ENCODER_CLK = 3;   // A2
static const int PIN_ENCODER_DT  = 4;   // A3
static const int PIN_ENCODER_SW  = 13;  // A6

// --- Per-Zone LEDs ---
static const int PIN_LED_ZONE1 = 14;  // A7
static const int PIN_LED_ZONE2 = 44;  // D0
static const int PIN_LED_ZONE3 = 43;  // D1
static const int PIN_LED_ZONE4 = 18;  // D7

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
