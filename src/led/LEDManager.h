#pragma once

#include "../hal/HAL.h"
#include "../zone/ZoneController.h"

class LEDManager {
public:
    LEDManager(IHAL& hal, const int* ledPins, int numZones);

    void begin();

    // Call every loop iteration with current zone states
    void update(const ZoneController& zoneCtrl);

private:
    IHAL&      _hal;
    const int* _ledPins;
    int        _numZones;
    unsigned long _lastBlinkToggle;
    bool       _blinkState;
};
