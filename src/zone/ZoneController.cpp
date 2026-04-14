#include "ZoneController.h"

ZoneController::ZoneController(IHAL& hal, IClock& clock, const int* relayPins, int numZones)
    : _hal(hal)
    , _clock(clock)
    , _relayPins(relayPins)
    , _numZones(numZones < ZONE_MAX ? numZones : ZONE_MAX)
    , _activeZone(0)
    , _zoneStartTime(0)
    , _zoneDurationMin(0)
{
    for (int i = 0; i < ZONE_MAX; i++) {
        _states[i] = ZoneState::IDLE;
    }
}

void ZoneController::begin() {
    for (int i = 0; i < _numZones; i++) {
        _hal.gpioMode(_relayPins[i], OUTPUT);
    }
    setAllRelaysOff();
}

void ZoneController::startQueue(const ZoneRunQueue& queue) {
    // Stop anything currently running
    if (_activeZone > 0) {
        deactivateCurrentZone(false);
    }

    // Reset all zone states
    for (int i = 0; i < ZONE_MAX; i++) {
        _states[i] = ZoneState::IDLE;
    }

    _queue = queue;
    _queue.currentIndex = 0;

    // Start the first zone in the queue
    if (_queue.hasNext()) {
        activateZone(*_queue.current());
        _queue.advance();
    }
}

void ZoneController::stopAll() {
    deactivateCurrentZone(false);
    setAllRelaysOff();
    _queue.clear();

    for (int i = 0; i < ZONE_MAX; i++) {
        _states[i] = ZoneState::IDLE;
    }
}

void ZoneController::update() {
    if (_activeZone == 0) return;

    time_t elapsed = _clock.now() - _zoneStartTime;
    if (elapsed >= _zoneDurationMin * 60) {
        // Current zone is done
        deactivateCurrentZone(true);

        // Start next zone in queue
        if (_queue.hasNext()) {
            activateZone(*_queue.current());
            _queue.advance();
        }
    }
}

ZoneState ZoneController::zoneState(int zoneId) const {
    if (zoneId < 1 || zoneId > ZONE_MAX) return ZoneState::IDLE;
    return _states[zoneId - 1];
}

int ZoneController::activeZone() const {
    return _activeZone;
}

bool ZoneController::isWatering() const {
    return _activeZone > 0;
}

int ZoneController::remainingSeconds() const {
    if (_activeZone == 0) return 0;
    time_t elapsed = _clock.now() - _zoneStartTime;
    int total = _zoneDurationMin * 60;
    int remaining = total - static_cast<int>(elapsed);
    return remaining > 0 ? remaining : 0;
}

void ZoneController::activateZone(const ZoneRun& run) {
    if (run.zoneId < 1 || run.zoneId > _numZones) return;

    // Ensure only one zone is active
    setAllRelaysOff();

    _activeZone = run.zoneId;
    _zoneDurationMin = run.durationMin;
    _zoneStartTime = _clock.now();
    _states[run.zoneId - 1] = ZoneState::ACTIVE;

    // Active LOW relay
    _hal.gpioWrite(_relayPins[run.zoneId - 1], LOW);

    _hal.serialPrint("Zone ");
    char buf[4];
    buf[0] = '0' + run.zoneId;
    buf[1] = '\0';
    _hal.serialPrint(buf);
    _hal.serialPrintln(" ON");
}

void ZoneController::deactivateCurrentZone(bool markCompleted) {
    if (_activeZone == 0) return;

    int idx = _activeZone - 1;
    _hal.gpioWrite(_relayPins[idx], HIGH);

    if (markCompleted) {
        _states[idx] = ZoneState::COMPLETED;
    } else {
        _states[idx] = ZoneState::IDLE;
    }

    _activeZone = 0;
    _zoneDurationMin = 0;
}

void ZoneController::setAllRelaysOff() {
    for (int i = 0; i < _numZones; i++) {
        _hal.gpioWrite(_relayPins[i], HIGH);
    }
}
