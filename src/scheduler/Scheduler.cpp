#include "Scheduler.h"
#include <cstdio>

const char* Scheduler::NVS_KEY = "schedules";

// --- ScheduleStore serialization ---
// Each slot: [enabled(1), daysOfWeek(1), hour(1), minute(1), zoneCount(1),
//             zone1_id(1), zone1_dur_hi(1), zone1_dur_lo(1),
//             zone2_id(1), zone2_dur_hi(1), zone2_dur_lo(1),
//             zone3_id(1), zone3_dur_hi(1), zone3_dur_lo(1),
//             zone4_id(1), zone4_dur_hi(1)] = 16 bytes (zone4_dur_lo dropped, we have 16 bytes)
// Actually let's use a cleaner layout: 14 bytes per slot with 2 padding

// Simplified: pack each slot into 16 bytes
static const int BYTES_PER_SLOT = 16;

void ScheduleStore::serialize(const Schedule slots[SCHEDULER_MAX_SLOTS]) {
    std::memset(data, 0, sizeof(data));
    for (int i = 0; i < SCHEDULER_MAX_SLOTS; i++) {
        uint8_t* p = &data[i * BYTES_PER_SLOT];
        const Schedule& s = slots[i];
        p[0] = s.enabled ? 1 : 0;
        p[1] = s.daysOfWeek;
        p[2] = s.startHour;
        p[3] = s.startMinute;
        p[4] = s.zoneCount;
        for (int z = 0; z < SCHEDULER_MAX_ZONES_PER_SLOT && z < s.zoneCount; z++) {
            int base = 5 + z * 3;
            if (base + 2 >= BYTES_PER_SLOT) break;
            p[base]     = s.zones[z].zoneId;
            p[base + 1] = (s.zones[z].durationMin >> 8) & 0xFF;
            p[base + 2] = s.zones[z].durationMin & 0xFF;
        }
    }
}

void ScheduleStore::deserialize(Schedule slots[SCHEDULER_MAX_SLOTS]) const {
    for (int i = 0; i < SCHEDULER_MAX_SLOTS; i++) {
        const uint8_t* p = &data[i * BYTES_PER_SLOT];
        Schedule& s = slots[i];
        s.clear();
        s.enabled     = p[0] != 0;
        s.daysOfWeek  = p[1];
        s.startHour   = p[2];
        s.startMinute = p[3];
        s.zoneCount   = p[4];
        if (s.zoneCount > SCHEDULER_MAX_ZONES_PER_SLOT) {
            s.zoneCount = SCHEDULER_MAX_ZONES_PER_SLOT;
        }
        for (int z = 0; z < s.zoneCount; z++) {
            int base = 5 + z * 3;
            if (base + 2 >= BYTES_PER_SLOT) break;
            s.zones[z].zoneId      = p[base];
            s.zones[z].durationMin = (p[base + 1] << 8) | p[base + 2];
        }
    }
}

// --- Scheduler ---

Scheduler::Scheduler(IHAL& hal, IClock& clock)
    : _hal(hal)
    , _clock(clock)
{
    for (int i = 0; i < SCHEDULER_MAX_SLOTS; i++) {
        _slots[i].clear();
        _lastTriggeredTime[i] = 0;
    }
}

void Scheduler::loadFromNVS() {
    ScheduleStore store;
    size_t readLen = 0;
    if (_hal.nvsRead(NVS_KEY, store.data, sizeof(store.data), &readLen)) {
        if (readLen == sizeof(store.data)) {
            store.deserialize(_slots);
            _hal.serialPrintln("Schedules loaded from NVS");
        }
    } else {
        _hal.serialPrintln("No saved schedules found");
    }
}

void Scheduler::saveToNVS() {
    ScheduleStore store;
    store.serialize(_slots);
    if (_hal.nvsWrite(NVS_KEY, store.data, sizeof(store.data))) {
        _hal.serialPrintln("Schedules saved to NVS");
    } else {
        _hal.serialPrintln("Failed to save schedules");
    }
}

bool Scheduler::checkSchedules(ZoneRunQueue& queue) {
    // Floor current time to the minute for dedup
    time_t now = _clock.now();
    time_t nowMinute = now - (now % 60);

    for (int i = 0; i < SCHEDULER_MAX_SLOTS; i++) {
        if (!matchesNow(_slots[i])) continue;

        // Prevent double-trigger: skip if we already fired this slot at this exact minute
        if (_lastTriggeredTime[i] == nowMinute) {
            continue;
        }

        // Build the zone queue
        queue.clear();
        for (int z = 0; z < _slots[i].zoneCount; z++) {
            queue.add(_slots[i].zones[z].zoneId, _slots[i].zones[z].durationMin);
        }

        _lastTriggeredTime[i] = nowMinute;
        return true;
    }

    return false;
}

const Schedule& Scheduler::getSchedule(int slot) const {
    static Schedule empty = {};
    if (slot < 0 || slot >= SCHEDULER_MAX_SLOTS) return empty;
    return _slots[slot];
}

bool Scheduler::setSchedule(int slot, const Schedule& schedule) {
    if (slot < 0 || slot >= SCHEDULER_MAX_SLOTS) return false;
    _slots[slot] = schedule;
    return true;
}

bool Scheduler::deleteSchedule(int slot) {
    if (slot < 0 || slot >= SCHEDULER_MAX_SLOTS) return false;
    _slots[slot].clear();
    return true;
}

bool Scheduler::toggleSchedule(int slot) {
    if (slot < 0 || slot >= SCHEDULER_MAX_SLOTS) return false;
    _slots[slot].enabled = !_slots[slot].enabled;
    return true;
}

void Scheduler::getNextScheduleText(char* buf, size_t bufLen) const {
    if (!buf || bufLen == 0) return;
    for (int i = 0; i < SCHEDULER_MAX_SLOTS; i++) {
        const Schedule& s = _slots[i];
        if (!s.enabled || s.zoneCount == 0) continue;
        int h = s.startHour;
        bool pm = h >= 12;
        int h12 = h % 12;
        if (h12 == 0) h12 = 12;
        snprintf(buf, bufLen, "%d:%02d %s", h12, s.startMinute, pm ? "PM" : "AM");
        return;
    }
    buf[0] = '\0';
}

bool Scheduler::matchesNow(const Schedule& sched) const {
    if (!sched.enabled) return false;
    if (sched.zoneCount == 0) return false;

    int dow = _clock.dayOfWeek(); // 0=Sunday
    if (!(sched.daysOfWeek & (1 << dow))) return false;

    if (_clock.hour() != sched.startHour) return false;
    if (_clock.minute() != sched.startMinute) return false;

    return true;
}
