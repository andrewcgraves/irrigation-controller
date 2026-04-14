#include "InputManager.h"
#include "../../include/config.h"
#include <cstdio>

InputManager::InputManager(IHAL& hal, int clkPin, int dtPin, int swPin)
    : _hal(hal)
    , _clkPin(clkPin)
    , _dtPin(dtPin)
    , _swPin(swPin)
    , _lastClk(HIGH)
    , _lastEncoderTime(0)
    , _buttonDown(false)
    , _buttonDownTime(0)
    , _longPressConsumed(false)
    , _lastActivity(0)
{}

void InputManager::begin() {
    _hal.gpioMode(_clkPin, INPUT_PULLUP);
    _hal.gpioMode(_dtPin, INPUT_PULLUP);
    _hal.gpioMode(_swPin, INPUT_PULLUP);
    _lastClk = _hal.gpioRead(_clkPin);

    char buf[40];
    snprintf(buf, sizeof(buf), "ENC init: CLK=%d DT=%d SW=%d",
             _lastClk,
             _hal.gpioRead(_dtPin),
             _hal.gpioRead(_swPin));
    _hal.serialPrintln(buf);
}

InputEvent InputManager::poll() {
    unsigned long now = _hal.millis();
    InputEvent event = InputEvent::None;

    // --- Rotary encoder ---
    int clk = _hal.gpioRead(_clkPin);
    if (clk != _lastClk && clk == LOW) {
        // Falling edge on CLK — debounce: ignore transitions within ENCODER_DEBOUNCE_MS
        // of the last accepted edge to prevent contact bounce from generating spurious events
        if (now - _lastEncoderTime >= ENCODER_DEBOUNCE_MS) {
            int dt = _hal.gpioRead(_dtPin);
            if (dt != clk) {
                event = InputEvent::RotateCW;
                _hal.serialPrintln("ENC: CW");
            } else {
                event = InputEvent::RotateCCW;
                _hal.serialPrintln("ENC: CCW");
            }
            _lastActivity = now;
            _lastEncoderTime = now;
        }
    }
    _lastClk = clk;

    // --- Push button (active LOW, pulled up) ---
    int sw = _hal.gpioRead(_swPin);
    bool pressed = (sw == LOW);

    if (pressed && !_buttonDown) {
        _buttonDown = true;
        _buttonDownTime = now;
        _longPressConsumed = false;
    } else if (pressed && _buttonDown && !_longPressConsumed) {
        if (now - _buttonDownTime >= LONG_PRESS_MS) {
            event = InputEvent::ButtonLongPress;
            _longPressConsumed = true;
            _lastActivity = now;
            _hal.serialPrintln("BTN: long press");
        }
    } else if (!pressed && _buttonDown) {
        if (!_longPressConsumed && (now - _buttonDownTime >= BUTTON_DEBOUNCE_MS)) {
            event = InputEvent::ButtonPress;
            _lastActivity = now;
            _hal.serialPrintln("BTN: press");
        }
        _buttonDown = false;
    }

    return event;
}
