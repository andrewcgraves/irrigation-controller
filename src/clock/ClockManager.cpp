#ifdef ARDUINO

#include "ClockManager.h"
#include <time.h>

void ClockManager::beginNTP(long gmtOffsetSec, int daylightOffsetSec) {
    configTime(gmtOffsetSec, daylightOffsetSec, "pool.ntp.org", "time.nist.gov");
}

struct tm ClockManager::_getTime() {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    return timeinfo;
}

time_t ClockManager::now() {
    time_t t;
    time(&t);
    return t;
}

int ClockManager::hour() {
    return _getTime().tm_hour;
}

int ClockManager::minute() {
    return _getTime().tm_min;
}

int ClockManager::second() {
    return _getTime().tm_sec;
}

int ClockManager::dayOfWeek() {
    return _getTime().tm_wday; // 0=Sunday
}

#endif // ARDUINO
