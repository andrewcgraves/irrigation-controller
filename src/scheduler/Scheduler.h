#pragma once

#include "../hal/HAL.h"
#include "../clock/ClockManager.h"
#include "../zone/ZoneController.h"
#include <cstdint>
#include <cstring>

static const int SCHEDULER_MAX_SLOTS = 8;
static const int SCHEDULER_MAX_ZONES_PER_SLOT = 4;

// Day-of-week bitmask: bit 0 = Sunday, bit 6 = Saturday
static const uint8_t DAY_SUN = (1 << 0);
static const uint8_t DAY_MON = (1 << 1);
static const uint8_t DAY_TUE = (1 << 2);
static const uint8_t DAY_WED = (1 << 3);
static const uint8_t DAY_THU = (1 << 4);
static const uint8_t DAY_FRI = (1 << 5);
static const uint8_t DAY_SAT = (1 << 6);
static const uint8_t DAY_EVERYDAY = 0x7F;

struct ScheduleZone {
    uint8_t  zoneId;      // 1-based
    uint16_t durationMin; // per-zone duration
};

struct Schedule {
    bool          enabled;
    uint8_t       daysOfWeek;   // bitmask
    uint8_t       startHour;    // 0-23
    uint8_t       startMinute;  // 0-59
    uint8_t       zoneCount;
    ScheduleZone  zones[SCHEDULER_MAX_ZONES_PER_SLOT];

    void clear() {
        enabled = false;
        daysOfWeek = 0;
        startHour = 0;
        startMinute = 0;
        zoneCount = 0;
        for (int i = 0; i < SCHEDULER_MAX_ZONES_PER_SLOT; i++) {
            zones[i] = {0, 0};
        }
    }
};

// Serialized format for NVS persistence (fixed-size, no padding issues)
struct ScheduleStore {
    uint8_t data[SCHEDULER_MAX_SLOTS * 16]; // 16 bytes per schedule slot

    void serialize(const Schedule slots[SCHEDULER_MAX_SLOTS]);
    void deserialize(Schedule slots[SCHEDULER_MAX_SLOTS]) const;
};

class Scheduler {
public:
    Scheduler(IHAL& hal, IClock& clock);

    // Load schedules from NVS
    void loadFromNVS();

    // Save schedules to NVS
    void saveToNVS();

    // Check if any schedule should fire right now.
    // Returns true and populates `queue` if a schedule matches.
    // Caller is responsible for starting the queue on ZoneController.
    bool checkSchedules(ZoneRunQueue& queue);

    // Schedule CRUD
    const Schedule& getSchedule(int slot) const; // 0-based slot index
    bool setSchedule(int slot, const Schedule& schedule);
    bool deleteSchedule(int slot);
    bool toggleSchedule(int slot);

    int slotCount() const { return SCHEDULER_MAX_SLOTS; }

    // Fills buf with a short display string for the next enabled schedule
    // (e.g. "8:00 AM"), or empty string if none.
    void getNextScheduleText(char* buf, size_t bufLen) const;

private:
    bool matchesNow(const Schedule& sched) const;

    IHAL&   _hal;
    IClock& _clock;
    Schedule _slots[SCHEDULER_MAX_SLOTS];

    // Prevent double-trigger: store the trigger time (floored to minute) per slot
    time_t _lastTriggeredTime[SCHEDULER_MAX_SLOTS];

    static const char* NVS_KEY;
};
