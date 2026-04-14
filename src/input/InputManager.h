#pragma once

#include "../hal/HAL.h"
#include <cstdint>

enum class InputEvent : uint8_t {
    None,
    RotateCW,
    RotateCCW,
    ButtonPress,
    ButtonLongPress
};

// Abstract input interface for testability
class IInputManager {
public:
    virtual ~IInputManager() = default;
    virtual InputEvent poll() = 0;
    virtual bool hasActivity() const = 0;
    virtual unsigned long lastActivityTime() const = 0;
};

class InputManager : public IInputManager {
public:
    InputManager(IHAL& hal, int clkPin, int dtPin, int swPin);

    void begin();

    // Call every loop iteration. Returns the latest input event (or None).
    InputEvent poll() override;

    // For idle timeout tracking
    bool hasActivity() const override { return _lastActivity > 0; }
    unsigned long lastActivityTime() const override { return _lastActivity; }

private:
    IHAL& _hal;
    int   _clkPin;
    int   _dtPin;
    int   _swPin;

    // Encoder state
    int  _lastClk;

    // Button state
    bool          _buttonDown;
    unsigned long _buttonDownTime;
    bool          _longPressConsumed;

    // Activity tracking
    unsigned long _lastActivity;
};
