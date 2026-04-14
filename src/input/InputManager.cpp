#include "InputManager.h"
#include "../../include/config.h"

InputManager::InputManager(IHAL& hal, int clkPin, int dtPin, int swPin)
    : _hal(hal)
    , _clkPin(clkPin)
    , _dtPin(dtPin)
    , _swPin(swPin)
    , _lastClk(HIGH)
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
}

InputEvent InputManager::poll() {
    unsigned long now = _hal.millis();
    InputEvent event = InputEvent::None;

    // --- Rotary encoder ---
    int clk = _hal.gpioRead(_clkPin);
    if (clk != _lastClk && clk == LOW) {
        // Falling edge on CLK — check DT to determine direction
        int dt = _hal.gpioRead(_dtPin);
        if (dt != clk) {
            event = InputEvent::RotateCW;
        } else {
            event = InputEvent::RotateCCW;
        }
        _lastActivity = now;
    }
    _lastClk = clk;

    // --- Push button (active LOW, pulled up) ---
    int sw = _hal.gpioRead(_swPin);
    bool pressed = (sw == LOW);

    if (pressed && !_buttonDown) {
        // Button just pressed
        _buttonDown = true;
        _buttonDownTime = now;
        _longPressConsumed = false;
    } else if (pressed && _buttonDown && !_longPressConsumed) {
        // Button held — check for long press
        if (now - _buttonDownTime >= LONG_PRESS_MS) {
            event = InputEvent::ButtonLongPress;
            _longPressConsumed = true;
            _lastActivity = now;
        }
    } else if (!pressed && _buttonDown) {
        // Button released
        if (!_longPressConsumed && (now - _buttonDownTime >= BUTTON_DEBOUNCE_MS)) {
            event = InputEvent::ButtonPress;
            _lastActivity = now;
        }
        _buttonDown = false;
    }

    return event;
}
