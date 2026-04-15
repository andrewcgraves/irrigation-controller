#pragma once

#include "../hal/HAL.h"
#include "../clock/ClockManager.h"
#include <cstdint>

static const int ZONE_MAX = 4;

enum class ZoneState : uint8_t {
    IDLE,
    ACTIVE,
    COMPLETED
};

struct ZoneRun {
    uint8_t  zoneId;      // 1-based zone number
    uint16_t durationMin; // watering duration in minutes
};

// Fixed-size queue replacing the malloc linked list
struct ZoneRunQueue {
    ZoneRun entries[ZONE_MAX];
    int     count = 0;
    int     currentIndex = 0;

    void clear() {
        count = 0;
        currentIndex = 0;
    }

    bool add(uint8_t zoneId, uint16_t durationMin) {
        if (count >= ZONE_MAX) return false;
        entries[count++] = {zoneId, durationMin};
        return true;
    }

    bool hasNext() const {
        return currentIndex < count;
    }

    const ZoneRun* current() const {
        if (currentIndex < count) return &entries[currentIndex];
        return nullptr;
    }

    void advance() {
        if (currentIndex < count) currentIndex++;
    }
};

class ZoneController {
public:
    ZoneController(IHAL& hal, IClock& clock, const int* relayPins, int numZones);

    // Initialize all relay pins to OFF
    void begin();

    // Start watering a queue of zones sequentially
    void startQueue(const ZoneRunQueue& queue);

    // Stop all watering immediately
    void stopAll();

    // Call every loop iteration — advances queue when zone duration expires
    void update();

    // Query state
    ZoneState zoneState(int zoneId) const;  // 1-based
    int       activeZone() const;           // 0 if none
    bool      isWatering() const;
    int       remainingSeconds() const;
    int       activeDurationSeconds() const;  // 0 if no zone active

private:
    void activateZone(const ZoneRun& run);
    void deactivateCurrentZone(bool markCompleted);
    void setAllRelaysOff();

    IHAL&   _hal;
    IClock& _clock;
    const int* _relayPins;
    int     _numZones;

    ZoneRunQueue _queue;
    ZoneState    _states[ZONE_MAX];
    int          _activeZone;     // 0 = none, 1-4 = active zone
    time_t       _zoneStartTime;
    uint16_t     _zoneDurationMin;
};
