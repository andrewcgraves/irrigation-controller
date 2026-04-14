#pragma once

#include <cstdint>
#include <ctime>

// Abstract clock interface for testability
class IClock {
public:
    virtual ~IClock() = default;

    virtual time_t now() = 0;
    virtual int    hour() = 0;     // 0-23
    virtual int    minute() = 0;   // 0-59
    virtual int    second() = 0;   // 0-59
    virtual int    dayOfWeek() = 0; // 0=Sunday, 6=Saturday
};

#ifdef ARDUINO

#include <Arduino.h>

class ClockManager : public IClock {
public:
    // Call once in setup() after WiFi is connected
    void beginNTP(long gmtOffsetSec, int daylightOffsetSec);

    time_t now() override;
    int    hour() override;
    int    minute() override;
    int    second() override;
    int    dayOfWeek() override;

private:
    struct tm _getTime();
};

#endif // ARDUINO
