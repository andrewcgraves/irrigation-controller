#pragma once

#include "../hal/HAL.h"
#include "../clock/ClockManager.h"

#ifdef ARDUINO
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#endif

// Forward declarations
struct Schedule;

class DisplayManager {
public:
#ifdef ARDUINO
    DisplayManager(IHAL& hal, IClock& clock,
                   int mosiPin, int clkPin, int dcPin, int rstPin, int csPin);
#else
    DisplayManager(IHAL& hal, IClock& clock);
#endif

    void begin();

    // Wake the display and reset idle timer
    void wake();

    // Turn off display after idle timeout
    void checkIdleTimeout();

    bool isOn() const { return _displayOn; }

    // --- Screen rendering methods ---

    void showHomeScreen(bool wifiConnected, bool mqttConnected,
                        const char* nextScheduleText);

    void showMenuItem(const char* title, const char* value);

    void showWateringProgress(int zoneId, int elapsedSec, int totalSec,
                              int nextZoneId);

    void showDaySelector(const char* title, uint8_t daysMask, int cursorPos);

    void showTimeEditor(const char* title, int hour, int minute, bool isPM,
                        int cursorField); // 0=hour, 1=minute, 2=ampm

    void showZoneSelector(const char* title, const bool zoneEnabled[4], int cursorPos);

    void showDurationEditor(int zoneId, int durationMin);

    void showConfirm(const char* question, bool yesSelected);

    void showMessage(const char* line1, const char* line2 = nullptr);

private:
    void clearAndDisplay();

    IHAL&   _hal;
    IClock& _clock;
    bool    _displayOn;
    unsigned long _lastWakeTime;

#ifdef ARDUINO
    Adafruit_SSD1306* _oled;
    int _mosiPin;
    int _clkPin;
    int _dcPin;
    int _rstPin;
    int _csPin;
#endif
};
