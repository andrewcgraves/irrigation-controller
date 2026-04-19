#include <unity.h>
#include "../../../src/state/StateMachine.h"
#include "../../../src/state/StateMachine.cpp"
#include "../../../src/zone/ZoneController.h"
#include "../../../src/zone/ZoneController.cpp"
#include "../../../src/scheduler/Scheduler.h"
#include "../../../src/scheduler/Scheduler.cpp"
#include "../../../src/ui/MenuSystem.h"
#include "../../../src/ui/MenuSystem.cpp"
#include "../../mocks/MockHAL.h"
#include "../../mocks/MockClock.h"

static MockHAL hal;
static MockClock clk;
static const int relayPins[4] = {5, 15, 16, 17};

static ZoneController* zoneCtrl;
static Scheduler* scheduler;
static MenuSystem* menu;
static StateMachine* sm;

// Track callback invocations
struct CallbackLog {
    int showHomeCount = 0;
    int showMenuCount = 0;
    int showWateringCount = 0;
    int showCompleteCount = 0;
    int displayWakeCount = 0;
    int displaySleepCount = 0;
    int publishOnCount = 0;
    int publishOffCount = 0;
    int publishZoneCount = 0;
    int saveSchedulesCount = 0;
    int lastWateringZone = 0;
    int lastWateringElapsed = 0;
    int lastWateringTotal = 0;
    bool lastZoneState = false;

    void reset() {
        showHomeCount = showMenuCount = showWateringCount = 0;
        showCompleteCount = displayWakeCount = displaySleepCount = 0;
        publishOnCount = publishOffCount = publishZoneCount = 0;
        saveSchedulesCount = 0;
        lastWateringZone = 0;
        lastWateringElapsed = lastWateringTotal = 0;
        lastZoneState = false;
    }
};

static CallbackLog cbLog;

class TestCallbacks : public IStateMachineCallbacks {
public:
    void onShowHome(bool, bool) override { cbLog.showHomeCount++; }
    void onShowMenu(Screen, const MenuSystem&) override { cbLog.showMenuCount++; }
    void onShowWatering(int z, int elapsed, int total, int) override { cbLog.showWateringCount++; cbLog.lastWateringZone = z; cbLog.lastWateringElapsed = elapsed; cbLog.lastWateringTotal = total; }
    void onShowComplete() override { cbLog.showCompleteCount++; }
    void onDisplayWake() override { cbLog.displayWakeCount++; }
    void onDisplaySleep() override { cbLog.displaySleepCount++; }
    void onPublishWateringOn(int z, int) override { cbLog.publishOnCount++; cbLog.lastWateringZone = z; }
    void onPublishWateringOff() override { cbLog.publishOffCount++; }
    void onPublishZoneState(int, bool s) override { cbLog.publishZoneCount++; cbLog.lastZoneState = s; }
    void onSaveSchedules() override { cbLog.saveSchedulesCount++; }
};

static TestCallbacks callbacks;

void setUp() {
    hal.reset();
    clk.setTime(2025, 1, 5, 6, 0, 0); // Sunday 06:00
    cbLog.reset();

    zoneCtrl = new ZoneController(hal, clk, relayPins, 4);
    zoneCtrl->begin();
    scheduler = new Scheduler(hal, clk);
    menu = new MenuSystem();
    sm = new StateMachine(hal, clk, *zoneCtrl, *scheduler, *menu);
    sm->setCallbacks(&callbacks);
    sm->begin();
    hal.writeLog.clear();
    cbLog.reset(); // clear begin() callbacks
}

void tearDown() {
    delete sm;
    delete menu;
    delete scheduler;
    delete zoneCtrl;
}

// Helpers
static void noInput() { sm->update(InputEvent::None, nullptr); }
static void press() { sm->update(InputEvent::ButtonPress, nullptr); }
static void longPress() { sm->update(InputEvent::ButtonLongPress, nullptr); }
static void rotateCW() { sm->update(InputEvent::RotateCW, nullptr); }
static void rotateCCW() { sm->update(InputEvent::RotateCCW, nullptr); }

static MQTTCommand makeOnCmd(uint8_t zone, uint16_t dur) {
    MQTTCommand cmd;
    cmd.type = MQTTCommandType::TurnOn;
    cmd.queue.clear();
    cmd.queue.add(zone, dur);
    return cmd;
}

static MQTTCommand makeOffCmd() {
    MQTTCommand cmd;
    cmd.type = MQTTCommandType::TurnOff;
    cmd.queue.clear();
    return cmd;
}

// ============================================================
// State Transitions
// ============================================================

void test_starts_in_idle() {
    TEST_ASSERT_EQUAL(AppState::Idle, sm->state());
}

void test_input_goes_to_menu_nav() {
    press(); // Home -> MainMenu (display already on)
    TEST_ASSERT_EQUAL(AppState::MenuNav, sm->state());
}

void test_menu_idle_timeout_returns_to_idle() {
    press(); // -> MenuNav
    TEST_ASSERT_EQUAL(AppState::MenuNav, sm->state());

    // Advance past menu timeout
    hal.advanceMillis(MENU_IDLE_TIMEOUT_MS + 1);
    noInput();
    TEST_ASSERT_EQUAL(AppState::Idle, sm->state());
}

void test_display_idle_timeout() {
    // In idle, advance past display timeout
    hal.advanceMillis(DISPLAY_IDLE_TIMEOUT_MS + 1);
    noInput();
    TEST_ASSERT_EQUAL(1, cbLog.displaySleepCount);
}

void test_input_wakes_display() {
    hal.advanceMillis(DISPLAY_IDLE_TIMEOUT_MS + 1);
    noInput(); // display sleeps
    cbLog.reset();

    press(); // wakes display
    TEST_ASSERT_EQUAL(1, cbLog.displayWakeCount);
}

// ============================================================
// MQTT Commands
// ============================================================

void test_mqtt_on_starts_watering() {
    MQTTCommand cmd = makeOnCmd(1, 10);
    sm->update(InputEvent::None, &cmd);

    TEST_ASSERT_EQUAL(AppState::Watering, sm->state());
    TEST_ASSERT_EQUAL(TriggerSource::MQTT, sm->triggerSource());
    TEST_ASSERT_TRUE(zoneCtrl->isWatering());
    TEST_ASSERT_EQUAL(1, zoneCtrl->activeZone());
    TEST_ASSERT_EQUAL(1, cbLog.publishOnCount);
}

void test_mqtt_off_stops_watering() {
    MQTTCommand onCmd = makeOnCmd(1, 10);
    sm->update(InputEvent::None, &onCmd);

    MQTTCommand offCmd = makeOffCmd();
    sm->update(InputEvent::None, &offCmd);

    TEST_ASSERT_EQUAL(AppState::Idle, sm->state());
    TEST_ASSERT_FALSE(zoneCtrl->isWatering());
    TEST_ASSERT_TRUE(cbLog.publishOffCount > 0);
}

void test_mqtt_on_replaces_schedule_watering() {
    // Start via schedule
    Schedule s;
    s.clear();
    s.enabled = true;
    s.daysOfWeek = DAY_SUN;
    s.startHour = 6;
    s.startMinute = 0;
    s.zoneCount = 1;
    s.zones[0] = {1, 10};
    scheduler->setSchedule(0, s);

    noInput(); // scheduler fires
    TEST_ASSERT_EQUAL(AppState::Watering, sm->state());
    TEST_ASSERT_EQUAL(TriggerSource::Schedule, sm->triggerSource());

    // MQTT replaces it
    MQTTCommand cmd = makeOnCmd(3, 5);
    sm->update(InputEvent::None, &cmd);

    TEST_ASSERT_EQUAL(AppState::Watering, sm->state());
    TEST_ASSERT_EQUAL(TriggerSource::MQTT, sm->triggerSource());
    TEST_ASSERT_EQUAL(3, zoneCtrl->activeZone());
}

void test_mqtt_off_from_any_state() {
    // In menu — one press goes from Idle(Home) -> MenuNav(MainMenu)
    press();
    TEST_ASSERT_EQUAL(AppState::MenuNav, sm->state());

    MQTTCommand offCmd = makeOffCmd();
    sm->update(InputEvent::None, &offCmd);
    // Should not crash, stays in menu
    TEST_ASSERT_FALSE(zoneCtrl->isWatering());
}

void test_mqtt_status_publishes() {
    MQTTCommand onCmd = makeOnCmd(2, 10);
    sm->update(InputEvent::None, &onCmd);
    cbLog.reset();

    MQTTCommand statusCmd;
    statusCmd.type = MQTTCommandType::GetStatus;
    statusCmd.queue.clear();
    sm->update(InputEvent::None, &statusCmd);

    TEST_ASSERT_EQUAL(1, cbLog.publishOnCount);
}

// ============================================================
// Scheduler Integration
// ============================================================

void test_scheduler_fires_from_idle() {
    Schedule s;
    s.clear();
    s.enabled = true;
    s.daysOfWeek = DAY_SUN;
    s.startHour = 6;
    s.startMinute = 0;
    s.zoneCount = 2;
    s.zones[0] = {1, 1};
    s.zones[1] = {2, 1};
    scheduler->setSchedule(0, s);

    noInput(); // scheduler check
    TEST_ASSERT_EQUAL(AppState::Watering, sm->state());
    TEST_ASSERT_EQUAL(TriggerSource::Schedule, sm->triggerSource());
    TEST_ASSERT_EQUAL(1, zoneCtrl->activeZone());
}

void test_scheduler_ignored_while_watering() {
    MQTTCommand cmd = makeOnCmd(3, 10);
    sm->update(InputEvent::None, &cmd);

    Schedule s;
    s.clear();
    s.enabled = true;
    s.daysOfWeek = DAY_SUN;
    s.startHour = 6;
    s.startMinute = 0;
    s.zoneCount = 1;
    s.zones[0] = {1, 10};
    scheduler->setSchedule(0, s);

    noInput(); // scheduler check — should not replace MQTT watering
    TEST_ASSERT_EQUAL(3, zoneCtrl->activeZone());
    TEST_ASSERT_EQUAL(TriggerSource::MQTT, sm->triggerSource());
}

// ============================================================
// Watering Completion
// ============================================================

void test_watering_completes_transitions_to_complete() {
    MQTTCommand cmd = makeOnCmd(1, 1); // 1 minute
    sm->update(InputEvent::None, &cmd);

    clk.advanceSeconds(60);
    zoneCtrl->update(); // zone finishes
    noInput();

    TEST_ASSERT_EQUAL(AppState::Complete, sm->state());
    TEST_ASSERT_EQUAL(1, cbLog.showCompleteCount);
}

void test_complete_returns_to_idle_after_timeout() {
    MQTTCommand cmd = makeOnCmd(1, 1);
    sm->update(InputEvent::None, &cmd);

    clk.advanceSeconds(60);
    zoneCtrl->update();
    noInput(); // -> Complete

    hal.advanceMillis(3001); // past COMPLETE_DISPLAY_MS
    noInput();

    TEST_ASSERT_EQUAL(AppState::Idle, sm->state());
}

// ============================================================
// Manual Watering via Menu
// ============================================================

void test_button_press_stops_watering() {
    MQTTCommand cmd = makeOnCmd(1, 10);
    sm->update(InputEvent::None, &cmd);
    TEST_ASSERT_EQUAL(AppState::Watering, sm->state());

    press(); // emergency stop
    TEST_ASSERT_EQUAL(AppState::Idle, sm->state());
    TEST_ASSERT_FALSE(zoneCtrl->isWatering());
}

void test_watering_display_passes_total_not_remaining() {
    MQTTCommand cmd = makeOnCmd(1, 2); // 2-minute zone
    sm->update(InputEvent::None, &cmd);
    cbLog.reset();

    clk.advanceSeconds(30); // 30s elapsed, 90s remaining
    noInput();

    TEST_ASSERT_EQUAL(120, cbLog.lastWateringTotal);   // full duration, not remaining
    TEST_ASSERT_EQUAL(30,  cbLog.lastWateringElapsed);
}

// ============================================================
// Menu Actions
// ============================================================

void test_save_schedule_triggers_callback() {
    // Navigate: Home -> MainMenu -> Schedules -> ScheduleList -> ScheduleAction -> Edit
    // First press: Idle(Home) -> MenuNav(MainMenu) at sel=0
    // But MainMenu sel=0 is "Manual Run", we need Schedules (sel=1)
    // So: press to enter menu, rotate to Schedules, press to select
    press(); // Idle(Home) -> MenuNav(MainMenu)
    rotateCW(); // sel=1 (Schedules)
    press(); // -> ScheduleList
    press(); // -> ScheduleAction (slot 0)
    press(); // -> EditDays (Edit selected)

    // Select Sunday
    press(); // toggle Sunday
    longPress(); // -> EditTimeHour

    press(); // -> EditTimeMinute
    press(); // -> EditTimeAMPM
    press(); // -> EditZoneSelect

    // Select zone 1
    press(); // toggle zone 1
    longPress(); // -> EditDuration

    press(); // -> EditConfirm
    press(); // confirm Yes -> SaveSchedule

    TEST_ASSERT_EQUAL(1, cbLog.saveSchedulesCount);
}

void test_toggle_schedule_triggers_callback() {
    // Set a schedule first — use different time to avoid scheduler firing
    Schedule s;
    s.clear();
    s.enabled = true;
    s.daysOfWeek = DAY_MON; // Monday, not Sunday (today is Sunday)
    s.startHour = 7;
    s.startMinute = 0;
    s.zoneCount = 1;
    s.zones[0] = {1, 10};
    scheduler->setSchedule(0, s);

    press(); // Idle(Home) -> MenuNav(MainMenu)
    rotateCW(); // Schedules
    press(); // -> ScheduleList
    press(); // -> ScheduleAction (slot 0)
    rotateCW(); // On/Off
    press(); // toggle

    TEST_ASSERT_EQUAL(1, cbLog.saveSchedulesCount);
    TEST_ASSERT_FALSE(scheduler->getSchedule(0).enabled);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // State transitions
    RUN_TEST(test_starts_in_idle);
    RUN_TEST(test_input_goes_to_menu_nav);
    RUN_TEST(test_menu_idle_timeout_returns_to_idle);
    RUN_TEST(test_display_idle_timeout);
    RUN_TEST(test_input_wakes_display);

    // MQTT
    RUN_TEST(test_mqtt_on_starts_watering);
    RUN_TEST(test_mqtt_off_stops_watering);
    RUN_TEST(test_mqtt_on_replaces_schedule_watering);
    RUN_TEST(test_mqtt_off_from_any_state);
    RUN_TEST(test_mqtt_status_publishes);

    // Scheduler
    RUN_TEST(test_scheduler_fires_from_idle);
    RUN_TEST(test_scheduler_ignored_while_watering);

    // Watering completion
    RUN_TEST(test_watering_completes_transitions_to_complete);
    RUN_TEST(test_complete_returns_to_idle_after_timeout);

    // Manual / Emergency stop
    RUN_TEST(test_button_press_stops_watering);
    RUN_TEST(test_watering_display_passes_total_not_remaining);

    // Menu actions
    RUN_TEST(test_save_schedule_triggers_callback);
    RUN_TEST(test_toggle_schedule_triggers_callback);

    return UNITY_END();
}
