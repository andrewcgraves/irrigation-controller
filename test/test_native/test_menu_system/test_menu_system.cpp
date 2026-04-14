#include <unity.h>
#include "../../../src/ui/MenuSystem.h"
#include "../../../src/ui/MenuSystem.cpp"
#include "../../../src/scheduler/Scheduler.h"

static MenuSystem* menu;

void setUp() {
    menu = new MenuSystem();
}

void tearDown() {
    delete menu;
}

// Helper: shorthand for events
static MenuAction press(MenuSystem* m) { return m->handleInput(InputEvent::ButtonPress); }
static MenuAction longPress(MenuSystem* m) { return m->handleInput(InputEvent::ButtonLongPress); }
static MenuAction cw(MenuSystem* m) { return m->handleInput(InputEvent::RotateCW); }
static MenuAction ccw(MenuSystem* m) { return m->handleInput(InputEvent::RotateCCW); }

// ============================================================
// Home Screen
// ============================================================

void test_starts_at_home() {
    TEST_ASSERT_EQUAL(Screen::Home, menu->currentScreen());
}

void test_any_input_goes_to_main_menu() {
    press(menu);
    TEST_ASSERT_EQUAL(Screen::MainMenu, menu->currentScreen());
}

void test_rotate_from_home_goes_to_menu() {
    cw(menu);
    TEST_ASSERT_EQUAL(Screen::MainMenu, menu->currentScreen());
}

// ============================================================
// Main Menu Navigation
// ============================================================

void test_main_menu_rotate_changes_selection() {
    press(menu); // Home -> MainMenu
    TEST_ASSERT_EQUAL(0, menu->selectionIndex());

    cw(menu);
    TEST_ASSERT_EQUAL(1, menu->selectionIndex());

    cw(menu);
    TEST_ASSERT_EQUAL(2, menu->selectionIndex());

    cw(menu);
    TEST_ASSERT_EQUAL(3, menu->selectionIndex());

    cw(menu); // wraps
    TEST_ASSERT_EQUAL(0, menu->selectionIndex());
}

void test_main_menu_rotate_ccw_wraps() {
    press(menu); // Home -> MainMenu
    ccw(menu);
    TEST_ASSERT_EQUAL(3, menu->selectionIndex());
}

void test_main_menu_long_press_goes_home() {
    press(menu); // Home -> MainMenu
    longPress(menu);
    TEST_ASSERT_EQUAL(Screen::Home, menu->currentScreen());
}

void test_main_menu_select_manual_run() {
    press(menu); // Home -> MainMenu (sel=0 = Manual Run)
    press(menu);
    TEST_ASSERT_EQUAL(Screen::ManualZoneSelect, menu->currentScreen());
}

void test_main_menu_select_schedules() {
    press(menu); // Home -> MainMenu
    cw(menu);    // sel=1 = Schedules
    press(menu);
    TEST_ASSERT_EQUAL(Screen::ScheduleList, menu->currentScreen());
}

void test_main_menu_select_settings() {
    press(menu);
    cw(menu); cw(menu); // sel=2
    press(menu);
    TEST_ASSERT_EQUAL(Screen::Settings, menu->currentScreen());
}

void test_main_menu_select_status() {
    press(menu);
    cw(menu); cw(menu); cw(menu); // sel=3
    press(menu);
    TEST_ASSERT_EQUAL(Screen::Status, menu->currentScreen());
}

// ============================================================
// Manual Run Flow
// ============================================================

void test_manual_zone_select_toggle() {
    press(menu); press(menu); // Home -> MainMenu -> ManualZoneSelect

    // Toggle zone 1 on
    press(menu);
    TEST_ASSERT_TRUE(menu->editZoneEnabled()[0]);

    // Toggle zone 1 off
    press(menu);
    TEST_ASSERT_FALSE(menu->editZoneEnabled()[0]);
}

void test_manual_zone_select_navigate() {
    press(menu); press(menu); // -> ManualZoneSelect

    cw(menu);
    TEST_ASSERT_EQUAL(1, menu->cursorPos());

    cw(menu);
    TEST_ASSERT_EQUAL(2, menu->cursorPos());
}

void test_manual_long_press_no_zones_goes_back() {
    press(menu); press(menu); // -> ManualZoneSelect
    // No zones selected, long press goes back
    longPress(menu);
    TEST_ASSERT_EQUAL(Screen::MainMenu, menu->currentScreen());
}

void test_manual_flow_complete() {
    press(menu); press(menu); // -> ManualZoneSelect

    // Select zone 1 and zone 3
    press(menu);          // toggle zone 1 on
    cw(menu); cw(menu);   // cursor to zone 3
    press(menu);          // toggle zone 3 on

    longPress(menu);       // confirm zones -> ManualDuration (zone 1)
    TEST_ASSERT_EQUAL(Screen::ManualDuration, menu->currentScreen());
    TEST_ASSERT_EQUAL(1, menu->editZoneId());

    // Set zone 1 duration to 15
    for (int i = 0; i < 5; i++) cw(menu); // 10 + 5 = 15
    TEST_ASSERT_EQUAL(15, menu->editDurationMin());

    press(menu); // advance to zone 3
    TEST_ASSERT_EQUAL(3, menu->editZoneId());

    // Set zone 3 duration to 8
    for (int i = 0; i < 2; i++) ccw(menu); // 10 - 2 = 8
    TEST_ASSERT_EQUAL(8, menu->editDurationMin());

    press(menu); // advance to confirm
    TEST_ASSERT_EQUAL(Screen::ManualConfirm, menu->currentScreen());

    // Confirm yes
    TEST_ASSERT_TRUE(menu->confirmYes());
    MenuAction action = press(menu);
    TEST_ASSERT_EQUAL(MenuAction::StartManualWatering, action);

    // Check the output queue
    ZoneRunQueue& q = menu->getManualQueue();
    TEST_ASSERT_EQUAL(2, q.count);
    TEST_ASSERT_EQUAL(1, q.entries[0].zoneId);
    TEST_ASSERT_EQUAL(15, q.entries[0].durationMin);
    TEST_ASSERT_EQUAL(3, q.entries[1].zoneId);
    TEST_ASSERT_EQUAL(8, q.entries[1].durationMin);

    TEST_ASSERT_EQUAL(Screen::Home, menu->currentScreen());
}

void test_manual_confirm_no_discards() {
    press(menu); press(menu); // -> ManualZoneSelect
    press(menu);              // toggle zone 1
    longPress(menu);          // -> ManualDuration
    press(menu);              // -> ManualConfirm

    cw(menu); // select No
    TEST_ASSERT_FALSE(menu->confirmYes());

    MenuAction action = press(menu);
    TEST_ASSERT_EQUAL(MenuAction::None, action);
    TEST_ASSERT_EQUAL(Screen::Home, menu->currentScreen());
}

void test_manual_duration_long_press_goes_back() {
    press(menu); press(menu); // -> ManualZoneSelect
    press(menu);              // toggle zone 1
    cw(menu); press(menu);    // toggle zone 2
    longPress(menu);          // -> ManualDuration (zone 1)

    press(menu);              // advance to zone 2
    TEST_ASSERT_EQUAL(2, menu->editZoneId());

    longPress(menu);          // back to zone 1
    TEST_ASSERT_EQUAL(1, menu->editZoneId());

    longPress(menu);          // back to zone select
    TEST_ASSERT_EQUAL(Screen::ManualZoneSelect, menu->currentScreen());
}

// ============================================================
// Schedule Editor Flow
// ============================================================

void test_schedule_list_navigation() {
    press(menu); cw(menu); press(menu); // -> ScheduleList
    TEST_ASSERT_EQUAL(Screen::ScheduleList, menu->currentScreen());
    TEST_ASSERT_EQUAL(0, menu->selectionIndex());

    cw(menu);
    TEST_ASSERT_EQUAL(1, menu->selectionIndex());

    // Wraps at 8
    for (int i = 0; i < 7; i++) cw(menu);
    TEST_ASSERT_EQUAL(0, menu->selectionIndex());
}

void test_schedule_action_screen() {
    press(menu); cw(menu); press(menu); // -> ScheduleList
    press(menu);                         // -> ScheduleAction (slot 0)
    TEST_ASSERT_EQUAL(Screen::ScheduleAction, menu->currentScreen());
    TEST_ASSERT_EQUAL(0, menu->cursorPos()); // Edit selected
}

void test_schedule_toggle_action() {
    press(menu); cw(menu); press(menu); // -> ScheduleList
    press(menu);                         // -> ScheduleAction

    cw(menu); // cursor to On/Off (pos 1)
    MenuAction action = press(menu);
    TEST_ASSERT_EQUAL(MenuAction::ToggleSchedule, action);
    TEST_ASSERT_EQUAL(Screen::ScheduleList, menu->currentScreen());
}

void test_schedule_delete_action() {
    press(menu); cw(menu); press(menu); // -> ScheduleList
    press(menu);                         // -> ScheduleAction

    cw(menu); cw(menu); // cursor to Delete (pos 2)
    MenuAction action = press(menu);
    TEST_ASSERT_EQUAL(MenuAction::DeleteSchedule, action);
}

void test_schedule_edit_full_flow() {
    press(menu); cw(menu); press(menu); // -> ScheduleList
    press(menu);                         // -> ScheduleAction (Edit selected)
    press(menu);                         // -> EditDays

    TEST_ASSERT_EQUAL(Screen::EditDays, menu->currentScreen());

    // Select Sunday (pos 0)
    press(menu);
    TEST_ASSERT_EQUAL(DAY_SUN, menu->editDaysMask());

    // Select Wednesday (pos 3)
    cw(menu); cw(menu); cw(menu);
    press(menu);
    TEST_ASSERT_EQUAL(DAY_SUN | DAY_WED, menu->editDaysMask());

    longPress(menu); // -> EditTimeHour
    TEST_ASSERT_EQUAL(Screen::EditTimeHour, menu->currentScreen());

    // Set hour to 7
    cw(menu);
    TEST_ASSERT_EQUAL(7, menu->editHour());

    press(menu); // -> EditTimeMinute
    TEST_ASSERT_EQUAL(Screen::EditTimeMinute, menu->currentScreen());

    // Set minute to 30 (6 * 5)
    for (int i = 0; i < 6; i++) cw(menu);
    TEST_ASSERT_EQUAL(30, menu->editMinute());

    press(menu); // -> EditTimeAMPM
    TEST_ASSERT_EQUAL(Screen::EditTimeAMPM, menu->currentScreen());

    // Set to PM
    cw(menu);
    TEST_ASSERT_TRUE(menu->editIsPM());

    press(menu); // -> EditZoneSelect
    TEST_ASSERT_EQUAL(Screen::EditZoneSelect, menu->currentScreen());

    // Select zone 2
    cw(menu); press(menu);
    TEST_ASSERT_TRUE(menu->editZoneEnabled()[1]);

    longPress(menu); // -> EditDuration (zone 2)
    TEST_ASSERT_EQUAL(Screen::EditDuration, menu->currentScreen());
    TEST_ASSERT_EQUAL(2, menu->editZoneId());

    // Set to 20 min
    for (int i = 0; i < 10; i++) cw(menu);
    TEST_ASSERT_EQUAL(20, menu->editDurationMin());

    press(menu); // -> EditConfirm
    TEST_ASSERT_EQUAL(Screen::EditConfirm, menu->currentScreen());

    MenuAction action = press(menu); // confirm Yes
    TEST_ASSERT_EQUAL(MenuAction::SaveSchedule, action);

    // Check the built schedule
    Schedule& s = menu->getEditingSchedule();
    TEST_ASSERT_TRUE(s.enabled);
    TEST_ASSERT_EQUAL(DAY_SUN | DAY_WED, s.daysOfWeek);
    TEST_ASSERT_EQUAL(19, s.startHour); // 7 PM = 19
    TEST_ASSERT_EQUAL(30, s.startMinute);
    TEST_ASSERT_EQUAL(1, s.zoneCount);
    TEST_ASSERT_EQUAL(2, s.zones[0].zoneId);
    TEST_ASSERT_EQUAL(20, s.zones[0].durationMin);
}

// ============================================================
// Settings / Status
// ============================================================

void test_settings_back_to_menu() {
    press(menu);
    cw(menu); cw(menu); // sel=2 Settings
    press(menu);
    TEST_ASSERT_EQUAL(Screen::Settings, menu->currentScreen());

    longPress(menu);
    TEST_ASSERT_EQUAL(Screen::MainMenu, menu->currentScreen());
}

void test_status_back_to_menu() {
    press(menu);
    cw(menu); cw(menu); cw(menu); // sel=3 Status
    press(menu);
    TEST_ASSERT_EQUAL(Screen::Status, menu->currentScreen());

    press(menu);
    TEST_ASSERT_EQUAL(Screen::MainMenu, menu->currentScreen());
}

// ============================================================
// Edge cases
// ============================================================

void test_none_event_does_nothing() {
    MenuAction action = menu->handleInput(InputEvent::None);
    TEST_ASSERT_EQUAL(MenuAction::None, action);
    TEST_ASSERT_EQUAL(Screen::Home, menu->currentScreen());
}

void test_duration_clamps_min() {
    press(menu); press(menu); // -> ManualZoneSelect
    press(menu);              // toggle zone 1
    longPress(menu);          // -> ManualDuration

    // Go below 1
    for (int i = 0; i < 20; i++) ccw(menu);
    TEST_ASSERT_EQUAL(1, menu->editDurationMin());
}

void test_duration_clamps_max() {
    press(menu); press(menu); // -> ManualZoneSelect
    press(menu);              // toggle zone 1
    longPress(menu);          // -> ManualDuration

    for (int i = 0; i < 50; i++) cw(menu);
    TEST_ASSERT_EQUAL(MAX_WATERING_DURATION_MIN, menu->editDurationMin());
}

void test_time_hour_wraps() {
    press(menu); cw(menu); press(menu); // -> ScheduleList
    press(menu); press(menu);            // -> EditDays
    press(menu); longPress(menu);        // select Sun, -> EditTimeHour

    // Hour starts at 6, wrap past 12
    for (int i = 0; i < 7; i++) cw(menu); // 6->7->8->9->10->11->12->1
    TEST_ASSERT_EQUAL(1, menu->editHour());
}

void test_time_minute_wraps() {
    press(menu); cw(menu); press(menu); // -> ScheduleList
    press(menu); press(menu);            // -> EditDays
    press(menu); longPress(menu);        // select Sun, -> EditTimeHour
    press(menu);                          // -> EditTimeMinute

    // Wrap past 55
    for (int i = 0; i < 12; i++) cw(menu); // 0->5->10->...->55->0
    TEST_ASSERT_EQUAL(0, menu->editMinute());
}

void test_edit_days_no_days_goes_back() {
    press(menu); cw(menu); press(menu); // -> ScheduleList
    press(menu); press(menu);            // -> EditDays

    // Long press with no days selected — go back
    longPress(menu);
    TEST_ASSERT_EQUAL(Screen::ScheduleList, menu->currentScreen());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Home
    RUN_TEST(test_starts_at_home);
    RUN_TEST(test_any_input_goes_to_main_menu);
    RUN_TEST(test_rotate_from_home_goes_to_menu);

    // Main Menu
    RUN_TEST(test_main_menu_rotate_changes_selection);
    RUN_TEST(test_main_menu_rotate_ccw_wraps);
    RUN_TEST(test_main_menu_long_press_goes_home);
    RUN_TEST(test_main_menu_select_manual_run);
    RUN_TEST(test_main_menu_select_schedules);
    RUN_TEST(test_main_menu_select_settings);
    RUN_TEST(test_main_menu_select_status);

    // Manual Run
    RUN_TEST(test_manual_zone_select_toggle);
    RUN_TEST(test_manual_zone_select_navigate);
    RUN_TEST(test_manual_long_press_no_zones_goes_back);
    RUN_TEST(test_manual_flow_complete);
    RUN_TEST(test_manual_confirm_no_discards);
    RUN_TEST(test_manual_duration_long_press_goes_back);

    // Schedule Editor
    RUN_TEST(test_schedule_list_navigation);
    RUN_TEST(test_schedule_action_screen);
    RUN_TEST(test_schedule_toggle_action);
    RUN_TEST(test_schedule_delete_action);
    RUN_TEST(test_schedule_edit_full_flow);

    // Settings / Status
    RUN_TEST(test_settings_back_to_menu);
    RUN_TEST(test_status_back_to_menu);

    // Edge cases
    RUN_TEST(test_none_event_does_nothing);
    RUN_TEST(test_duration_clamps_min);
    RUN_TEST(test_duration_clamps_max);
    RUN_TEST(test_time_hour_wraps);
    RUN_TEST(test_time_minute_wraps);
    RUN_TEST(test_edit_days_no_days_goes_back);

    return UNITY_END();
}
