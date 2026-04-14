#include <unity.h>
#include "../../../src/scheduler/Scheduler.h"
#include "../../../src/scheduler/Scheduler.cpp"
#include "../../../src/zone/ZoneController.h"
#include "../../mocks/MockHAL.h"
#include "../../mocks/MockClock.h"

static MockHAL hal;
static MockClock clk;
static Scheduler* sched;

// Helper to create a simple schedule
static Schedule makeSchedule(uint8_t days, uint8_t hour, uint8_t minute,
                              uint8_t zone1, uint16_t dur1) {
    Schedule s;
    s.clear();
    s.enabled = true;
    s.daysOfWeek = days;
    s.startHour = hour;
    s.startMinute = minute;
    s.zoneCount = 1;
    s.zones[0] = {zone1, dur1};
    return s;
}

static Schedule makeMultiZoneSchedule(uint8_t days, uint8_t hour, uint8_t minute) {
    Schedule s;
    s.clear();
    s.enabled = true;
    s.daysOfWeek = days;
    s.startHour = hour;
    s.startMinute = minute;
    s.zoneCount = 3;
    s.zones[0] = {1, 10};
    s.zones[1] = {2, 15};
    s.zones[2] = {3, 20};
    return s;
}

void setUp() {
    hal.reset();
    clk.setTime(2025, 1, 5, 6, 0, 0); // Sunday 06:00
    sched = new Scheduler(hal, clk);
}

void tearDown() {
    delete sched;
}

// --- Basic matching ---

void test_schedule_fires_on_matching_day_and_time() {
    // Sunday 06:00
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 0, 1, 10));

    ZoneRunQueue q;
    TEST_ASSERT_TRUE(sched->checkSchedules(q));
    TEST_ASSERT_EQUAL(1, q.count);
    TEST_ASSERT_EQUAL(1, q.entries[0].zoneId);
    TEST_ASSERT_EQUAL(10, q.entries[0].durationMin);
}

void test_schedule_does_not_fire_on_wrong_day() {
    // Monday only, but it's Sunday
    sched->setSchedule(0, makeSchedule(DAY_MON, 6, 0, 1, 10));

    ZoneRunQueue q;
    TEST_ASSERT_FALSE(sched->checkSchedules(q));
}

void test_schedule_does_not_fire_at_wrong_hour() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 7, 0, 1, 10)); // 07:00 not 06:00

    ZoneRunQueue q;
    TEST_ASSERT_FALSE(sched->checkSchedules(q));
}

void test_schedule_does_not_fire_at_wrong_minute() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 30, 1, 10)); // 06:30 not 06:00

    ZoneRunQueue q;
    TEST_ASSERT_FALSE(sched->checkSchedules(q));
}

// --- Multi-day bitmask ---

void test_schedule_fires_on_multiple_days() {
    // Sunday and Wednesday
    sched->setSchedule(0, makeSchedule(DAY_SUN | DAY_WED, 6, 0, 1, 10));

    ZoneRunQueue q;
    TEST_ASSERT_TRUE(sched->checkSchedules(q)); // Sunday

    // Advance to Wednesday 06:00 — different day, should fire again
    clk.setTime(2025, 1, 8, 6, 0, 0); // Wednesday

    ZoneRunQueue q2;
    TEST_ASSERT_TRUE(sched->checkSchedules(q2));
}

void test_everyday_schedule() {
    sched->setSchedule(0, makeSchedule(DAY_EVERYDAY, 6, 0, 1, 10));

    ZoneRunQueue q;
    TEST_ASSERT_TRUE(sched->checkSchedules(q));
}

// --- Disabled schedules ---

void test_disabled_schedule_does_not_fire() {
    Schedule s = makeSchedule(DAY_SUN, 6, 0, 1, 10);
    s.enabled = false;
    sched->setSchedule(0, s);

    ZoneRunQueue q;
    TEST_ASSERT_FALSE(sched->checkSchedules(q));
}

void test_toggle_schedule() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 0, 1, 10));
    TEST_ASSERT_TRUE(sched->getSchedule(0).enabled);

    sched->toggleSchedule(0);
    TEST_ASSERT_FALSE(sched->getSchedule(0).enabled);

    sched->toggleSchedule(0);
    TEST_ASSERT_TRUE(sched->getSchedule(0).enabled);
}

// --- No double-trigger ---

void test_no_double_trigger_same_minute() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 0, 1, 10));

    ZoneRunQueue q1;
    TEST_ASSERT_TRUE(sched->checkSchedules(q1));

    // Same minute, should not fire again
    ZoneRunQueue q2;
    TEST_ASSERT_FALSE(sched->checkSchedules(q2));
}

void test_fires_again_next_minute() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 0, 1, 10));

    ZoneRunQueue q1;
    sched->checkSchedules(q1);

    // Advance to 06:01 — different minute, but schedule is for 06:00 so should NOT fire
    clk.advanceSeconds(60);
    ZoneRunQueue q2;
    TEST_ASSERT_FALSE(sched->checkSchedules(q2));
}

void test_fires_again_next_week() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 0, 1, 10));

    ZoneRunQueue q1;
    sched->checkSchedules(q1);

    // Advance to next Sunday 06:00
    clk.setTime(2025, 1, 12, 6, 0, 0);
    ZoneRunQueue q2;
    TEST_ASSERT_TRUE(sched->checkSchedules(q2));
}

// --- Multi-zone queue ---

void test_multi_zone_schedule_builds_correct_queue() {
    sched->setSchedule(0, makeMultiZoneSchedule(DAY_SUN, 6, 0));

    ZoneRunQueue q;
    TEST_ASSERT_TRUE(sched->checkSchedules(q));
    TEST_ASSERT_EQUAL(3, q.count);
    TEST_ASSERT_EQUAL(1, q.entries[0].zoneId);
    TEST_ASSERT_EQUAL(10, q.entries[0].durationMin);
    TEST_ASSERT_EQUAL(2, q.entries[1].zoneId);
    TEST_ASSERT_EQUAL(15, q.entries[1].durationMin);
    TEST_ASSERT_EQUAL(3, q.entries[2].zoneId);
    TEST_ASSERT_EQUAL(20, q.entries[2].durationMin);
}

// --- Empty schedule ---

void test_empty_schedule_does_not_fire() {
    Schedule s;
    s.clear();
    s.enabled = true;
    s.daysOfWeek = DAY_SUN;
    s.startHour = 6;
    s.startMinute = 0;
    s.zoneCount = 0; // no zones
    sched->setSchedule(0, s);

    ZoneRunQueue q;
    TEST_ASSERT_FALSE(sched->checkSchedules(q));
}

// --- CRUD operations ---

void test_delete_schedule() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 0, 1, 10));
    TEST_ASSERT_TRUE(sched->getSchedule(0).enabled);

    sched->deleteSchedule(0);
    TEST_ASSERT_FALSE(sched->getSchedule(0).enabled);
    TEST_ASSERT_EQUAL(0, sched->getSchedule(0).zoneCount);
}

void test_invalid_slot_returns_false() {
    Schedule s = makeSchedule(DAY_SUN, 6, 0, 1, 10);
    TEST_ASSERT_FALSE(sched->setSchedule(-1, s));
    TEST_ASSERT_FALSE(sched->setSchedule(8, s));
    TEST_ASSERT_FALSE(sched->deleteSchedule(-1));
    TEST_ASSERT_FALSE(sched->toggleSchedule(99));
}

// --- Multiple slots, first match wins ---

void test_first_matching_slot_wins() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 0, 1, 5));
    sched->setSchedule(1, makeSchedule(DAY_SUN, 6, 0, 2, 10));

    ZoneRunQueue q;
    TEST_ASSERT_TRUE(sched->checkSchedules(q));
    TEST_ASSERT_EQUAL(1, q.entries[0].zoneId); // slot 0 wins
    TEST_ASSERT_EQUAL(5, q.entries[0].durationMin);
}

// --- Serialize / Deserialize roundtrip ---

void test_serialize_deserialize_roundtrip() {
    Schedule original[SCHEDULER_MAX_SLOTS];
    for (int i = 0; i < SCHEDULER_MAX_SLOTS; i++) original[i].clear();

    original[0] = makeSchedule(DAY_SUN | DAY_WED, 6, 30, 1, 10);
    original[2] = makeMultiZoneSchedule(DAY_MON | DAY_FRI, 7, 15);
    original[7] = makeSchedule(DAY_SAT, 18, 45, 4, 30);

    ScheduleStore store;
    store.serialize(original);

    Schedule loaded[SCHEDULER_MAX_SLOTS];
    store.deserialize(loaded);

    // Slot 0
    TEST_ASSERT_TRUE(loaded[0].enabled);
    TEST_ASSERT_EQUAL(DAY_SUN | DAY_WED, loaded[0].daysOfWeek);
    TEST_ASSERT_EQUAL(6, loaded[0].startHour);
    TEST_ASSERT_EQUAL(30, loaded[0].startMinute);
    TEST_ASSERT_EQUAL(1, loaded[0].zoneCount);
    TEST_ASSERT_EQUAL(1, loaded[0].zones[0].zoneId);
    TEST_ASSERT_EQUAL(10, loaded[0].zones[0].durationMin);

    // Slot 1 — empty
    TEST_ASSERT_FALSE(loaded[1].enabled);

    // Slot 2 — multi-zone
    TEST_ASSERT_TRUE(loaded[2].enabled);
    TEST_ASSERT_EQUAL(DAY_MON | DAY_FRI, loaded[2].daysOfWeek);
    TEST_ASSERT_EQUAL(7, loaded[2].startHour);
    TEST_ASSERT_EQUAL(15, loaded[2].startMinute);
    TEST_ASSERT_EQUAL(3, loaded[2].zoneCount);
    TEST_ASSERT_EQUAL(1, loaded[2].zones[0].zoneId);
    TEST_ASSERT_EQUAL(10, loaded[2].zones[0].durationMin);
    TEST_ASSERT_EQUAL(2, loaded[2].zones[1].zoneId);
    TEST_ASSERT_EQUAL(15, loaded[2].zones[1].durationMin);
    TEST_ASSERT_EQUAL(3, loaded[2].zones[2].zoneId);
    TEST_ASSERT_EQUAL(20, loaded[2].zones[2].durationMin);

    // Slot 7
    TEST_ASSERT_TRUE(loaded[7].enabled);
    TEST_ASSERT_EQUAL(DAY_SAT, loaded[7].daysOfWeek);
    TEST_ASSERT_EQUAL(18, loaded[7].startHour);
    TEST_ASSERT_EQUAL(45, loaded[7].startMinute);
    TEST_ASSERT_EQUAL(4, loaded[7].zones[0].zoneId);
    TEST_ASSERT_EQUAL(30, loaded[7].zones[0].durationMin);
}

// --- NVS persistence roundtrip ---

void test_nvs_save_and_load_roundtrip() {
    sched->setSchedule(0, makeSchedule(DAY_SUN, 6, 0, 1, 10));
    sched->setSchedule(3, makeMultiZoneSchedule(DAY_MON | DAY_FRI, 7, 15));
    sched->saveToNVS();

    // Create a new scheduler and load from the same mock NVS
    Scheduler sched2(hal, clk);
    sched2.loadFromNVS();

    const Schedule& s0 = sched2.getSchedule(0);
    TEST_ASSERT_TRUE(s0.enabled);
    TEST_ASSERT_EQUAL(DAY_SUN, s0.daysOfWeek);
    TEST_ASSERT_EQUAL(6, s0.startHour);
    TEST_ASSERT_EQUAL(1, s0.zones[0].zoneId);

    const Schedule& s3 = sched2.getSchedule(3);
    TEST_ASSERT_TRUE(s3.enabled);
    TEST_ASSERT_EQUAL(3, s3.zoneCount);
    TEST_ASSERT_EQUAL(2, s3.zones[1].zoneId);
    TEST_ASSERT_EQUAL(15, s3.zones[1].durationMin);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_schedule_fires_on_matching_day_and_time);
    RUN_TEST(test_schedule_does_not_fire_on_wrong_day);
    RUN_TEST(test_schedule_does_not_fire_at_wrong_hour);
    RUN_TEST(test_schedule_does_not_fire_at_wrong_minute);

    RUN_TEST(test_schedule_fires_on_multiple_days);
    RUN_TEST(test_everyday_schedule);

    RUN_TEST(test_disabled_schedule_does_not_fire);
    RUN_TEST(test_toggle_schedule);

    RUN_TEST(test_no_double_trigger_same_minute);
    RUN_TEST(test_fires_again_next_minute);
    RUN_TEST(test_fires_again_next_week);

    RUN_TEST(test_multi_zone_schedule_builds_correct_queue);
    RUN_TEST(test_empty_schedule_does_not_fire);

    RUN_TEST(test_delete_schedule);
    RUN_TEST(test_invalid_slot_returns_false);
    RUN_TEST(test_first_matching_slot_wins);

    RUN_TEST(test_serialize_deserialize_roundtrip);
    RUN_TEST(test_nvs_save_and_load_roundtrip);

    return UNITY_END();
}
