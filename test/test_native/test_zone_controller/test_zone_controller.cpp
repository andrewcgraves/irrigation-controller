#include <unity.h>
#include "../../../src/zone/ZoneController.h"
#include "../../../src/zone/ZoneController.cpp"
#include "../../mocks/MockHAL.h"
#include "../../mocks/MockClock.h"

static MockHAL hal;
static MockClock clk;

static const int testRelayPins[4] = {5, 15, 16, 17};
static ZoneController* zc;

void setUp() {
    hal.reset();
    clk.setTime(2025, 1, 5, 6, 0, 0);
    zc = new ZoneController(hal, clk, testRelayPins, 4);
    zc->begin();
    hal.writeLog.clear(); // clear setup writes
}

void tearDown() {
    delete zc;
}

// --- begin() tests ---

void test_begin_sets_all_relays_off() {
    MockHAL h;
    MockClock c;
    ZoneController z(h, c, testRelayPins, 4);
    z.begin();

    TEST_ASSERT_EQUAL(HIGH, h.pinValue(5));
    TEST_ASSERT_EQUAL(HIGH, h.pinValue(15));
    TEST_ASSERT_EQUAL(HIGH, h.pinValue(16));
    TEST_ASSERT_EQUAL(HIGH, h.pinValue(17));
}

void test_begin_sets_pin_modes_to_output() {
    MockHAL h;
    MockClock c;
    ZoneController z(h, c, testRelayPins, 4);
    z.begin();

    TEST_ASSERT_EQUAL(OUTPUT, h.pins[5].mode);
    TEST_ASSERT_EQUAL(OUTPUT, h.pins[15].mode);
    TEST_ASSERT_EQUAL(OUTPUT, h.pins[16].mode);
    TEST_ASSERT_EQUAL(OUTPUT, h.pins[17].mode);
}

// --- startQueue() tests ---

void test_start_single_zone_writes_low() {
    ZoneRunQueue q;
    q.add(1, 10);
    zc->startQueue(q);

    TEST_ASSERT_EQUAL(LOW, hal.pinValue(5));   // Zone 1 ON
    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(15));  // Zone 2 OFF
    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(16));  // Zone 3 OFF
    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(17));  // Zone 4 OFF
}

void test_start_zone2_writes_correct_pin() {
    ZoneRunQueue q;
    q.add(2, 10);
    zc->startQueue(q);

    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(5));   // Zone 1 OFF
    TEST_ASSERT_EQUAL(LOW, hal.pinValue(15));    // Zone 2 ON
}

void test_active_zone_reports_correctly() {
    ZoneRunQueue q;
    q.add(3, 10);
    zc->startQueue(q);

    TEST_ASSERT_EQUAL(3, zc->activeZone());
    TEST_ASSERT_TRUE(zc->isWatering());
}

void test_zone_state_is_active_when_watering() {
    ZoneRunQueue q;
    q.add(1, 10);
    zc->startQueue(q);

    TEST_ASSERT_EQUAL(ZoneState::ACTIVE, zc->zoneState(1));
    TEST_ASSERT_EQUAL(ZoneState::IDLE, zc->zoneState(2));
}

// --- Only one zone active at a time ---

void test_starting_new_queue_stops_current_zone() {
    ZoneRunQueue q1;
    q1.add(1, 10);
    zc->startQueue(q1);
    TEST_ASSERT_EQUAL(LOW, hal.pinValue(5));

    ZoneRunQueue q2;
    q2.add(2, 10);
    zc->startQueue(q2);

    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(5));  // Zone 1 stopped
    TEST_ASSERT_EQUAL(LOW, hal.pinValue(15));   // Zone 2 started
}

// --- update() auto-advance tests ---

void test_zone_auto_stops_after_duration() {
    ZoneRunQueue q;
    q.add(1, 1); // 1 minute
    zc->startQueue(q);

    TEST_ASSERT_EQUAL(LOW, hal.pinValue(5));

    clk.advanceSeconds(60); // 1 minute passes
    zc->update();

    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(5));
    TEST_ASSERT_FALSE(zc->isWatering());
}

void test_zone_marked_completed_after_duration() {
    ZoneRunQueue q;
    q.add(1, 1);
    zc->startQueue(q);

    clk.advanceSeconds(60);
    zc->update();

    TEST_ASSERT_EQUAL(ZoneState::COMPLETED, zc->zoneState(1));
}

void test_queue_advances_to_next_zone() {
    ZoneRunQueue q;
    q.add(1, 1);
    q.add(2, 1);
    zc->startQueue(q);

    TEST_ASSERT_EQUAL(1, zc->activeZone());

    clk.advanceSeconds(60);
    zc->update();

    TEST_ASSERT_EQUAL(2, zc->activeZone());
    TEST_ASSERT_EQUAL(LOW, hal.pinValue(15));
    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(5));
    TEST_ASSERT_EQUAL(ZoneState::COMPLETED, zc->zoneState(1));
    TEST_ASSERT_EQUAL(ZoneState::ACTIVE, zc->zoneState(2));
}

void test_full_queue_completes_all_zones() {
    ZoneRunQueue q;
    q.add(1, 1);
    q.add(2, 1);
    q.add(3, 1);
    zc->startQueue(q);

    // Zone 1 finishes
    clk.advanceSeconds(60);
    zc->update();
    TEST_ASSERT_EQUAL(2, zc->activeZone());

    // Zone 2 finishes
    clk.advanceSeconds(60);
    zc->update();
    TEST_ASSERT_EQUAL(3, zc->activeZone());

    // Zone 3 finishes
    clk.advanceSeconds(60);
    zc->update();
    TEST_ASSERT_EQUAL(0, zc->activeZone());
    TEST_ASSERT_FALSE(zc->isWatering());

    TEST_ASSERT_EQUAL(ZoneState::COMPLETED, zc->zoneState(1));
    TEST_ASSERT_EQUAL(ZoneState::COMPLETED, zc->zoneState(2));
    TEST_ASSERT_EQUAL(ZoneState::COMPLETED, zc->zoneState(3));
}

// --- stopAll() tests ---

void test_stop_all_turns_off_everything() {
    ZoneRunQueue q;
    q.add(1, 10);
    q.add(2, 10);
    zc->startQueue(q);

    zc->stopAll();

    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(5));
    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(15));
    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(16));
    TEST_ASSERT_EQUAL(HIGH, hal.pinValue(17));
    TEST_ASSERT_EQUAL(0, zc->activeZone());
    TEST_ASSERT_FALSE(zc->isWatering());
}

void test_stop_all_resets_zone_states_to_idle() {
    ZoneRunQueue q;
    q.add(1, 1);
    q.add(2, 1);
    zc->startQueue(q);

    clk.advanceSeconds(60);
    zc->update(); // zone 1 completed, zone 2 active

    zc->stopAll();

    TEST_ASSERT_EQUAL(ZoneState::IDLE, zc->zoneState(1));
    TEST_ASSERT_EQUAL(ZoneState::IDLE, zc->zoneState(2));
}

// --- remainingSeconds() tests ---

void test_remaining_seconds_counts_down() {
    ZoneRunQueue q;
    q.add(1, 10); // 10 minutes = 600 seconds
    zc->startQueue(q);

    TEST_ASSERT_EQUAL(600, zc->remainingSeconds());

    clk.advanceSeconds(100);
    TEST_ASSERT_EQUAL(500, zc->remainingSeconds());
}

void test_remaining_seconds_zero_when_not_watering() {
    TEST_ASSERT_EQUAL(0, zc->remainingSeconds());
}

// --- Edge cases ---

void test_update_does_nothing_when_not_watering() {
    size_t logBefore = hal.writeLog.size();
    zc->update();
    TEST_ASSERT_EQUAL(logBefore, hal.writeLog.size());
}

void test_invalid_zone_id_ignored() {
    ZoneRunQueue q;
    q.add(0, 10); // invalid
    zc->startQueue(q);

    TEST_ASSERT_EQUAL(0, zc->activeZone());
    TEST_ASSERT_FALSE(zc->isWatering());
}

void test_zone_run_queue_max_capacity() {
    ZoneRunQueue q;
    TEST_ASSERT_TRUE(q.add(1, 10));
    TEST_ASSERT_TRUE(q.add(2, 10));
    TEST_ASSERT_TRUE(q.add(3, 10));
    TEST_ASSERT_TRUE(q.add(4, 10));
    TEST_ASSERT_FALSE(q.add(5, 10)); // should fail, queue full
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_begin_sets_all_relays_off);
    RUN_TEST(test_begin_sets_pin_modes_to_output);

    RUN_TEST(test_start_single_zone_writes_low);
    RUN_TEST(test_start_zone2_writes_correct_pin);
    RUN_TEST(test_active_zone_reports_correctly);
    RUN_TEST(test_zone_state_is_active_when_watering);

    RUN_TEST(test_starting_new_queue_stops_current_zone);

    RUN_TEST(test_zone_auto_stops_after_duration);
    RUN_TEST(test_zone_marked_completed_after_duration);
    RUN_TEST(test_queue_advances_to_next_zone);
    RUN_TEST(test_full_queue_completes_all_zones);

    RUN_TEST(test_stop_all_turns_off_everything);
    RUN_TEST(test_stop_all_resets_zone_states_to_idle);

    RUN_TEST(test_remaining_seconds_counts_down);
    RUN_TEST(test_remaining_seconds_zero_when_not_watering);

    RUN_TEST(test_update_does_nothing_when_not_watering);
    RUN_TEST(test_invalid_zone_id_ignored);
    RUN_TEST(test_zone_run_queue_max_capacity);

    return UNITY_END();
}
