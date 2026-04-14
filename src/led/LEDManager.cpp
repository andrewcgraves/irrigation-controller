#include "LEDManager.h"
#include "../../include/config.h"

LEDManager::LEDManager(IHAL& hal, const int* ledPins, int numZones)
    : _hal(hal)
    , _ledPins(ledPins)
    , _numZones(numZones < ZONE_MAX ? numZones : ZONE_MAX)
    , _lastBlinkToggle(0)
    , _blinkState(false)
{}

void LEDManager::begin() {
    for (int i = 0; i < _numZones; i++) {
        _hal.gpioMode(_ledPins[i], OUTPUT);
        _hal.gpioWrite(_ledPins[i], LOW);
    }
}

void LEDManager::update(const ZoneController& zoneCtrl) {
    // Update blink state
    unsigned long now = _hal.millis();
    if (now - _lastBlinkToggle >= LED_BLINK_INTERVAL_MS) {
        _blinkState = !_blinkState;
        _lastBlinkToggle = now;
    }

    for (int i = 0; i < _numZones; i++) {
        ZoneState state = zoneCtrl.zoneState(i + 1); // 1-based

        switch (state) {
            case ZoneState::ACTIVE:
                // Blink when actively watering
                _hal.gpioWrite(_ledPins[i], _blinkState ? HIGH : LOW);
                break;
            case ZoneState::COMPLETED:
                // Solid on when completed
                _hal.gpioWrite(_ledPins[i], HIGH);
                break;
            case ZoneState::IDLE:
            default:
                // Off when inactive
                _hal.gpioWrite(_ledPins[i], LOW);
                break;
        }
    }
}
