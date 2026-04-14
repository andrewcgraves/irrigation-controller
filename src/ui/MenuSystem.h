#pragma once

#include "../input/InputManager.h"
#include "../scheduler/Scheduler.h"
#include "../zone/ZoneController.h"
#include <cstdint>

enum class Screen : uint8_t {
    Home,
    MainMenu,
    // Manual run flow
    ManualZoneSelect,
    ManualDuration,      // per-zone duration, loops through selected zones
    ManualConfirm,
    // Schedule management
    ScheduleList,
    ScheduleAction,      // Edit / On/Off / Delete
    // Schedule editor flow
    EditDays,
    EditTimeHour,
    EditTimeMinute,
    EditTimeAMPM,
    EditZoneSelect,
    EditDuration,        // per-zone duration, loops through selected zones
    EditConfirm,
    // Other screens
    Settings,
    Status
};

// Result actions that the main loop should handle
enum class MenuAction : uint8_t {
    None,
    StartManualWatering,  // queue is ready in getManualQueue()
    SaveSchedule,         // schedule is ready in getEditingSchedule()
    DeleteSchedule,
    ToggleSchedule
};

static const int MAIN_MENU_ITEMS = 4;
static const char* const MAIN_MENU_LABELS[MAIN_MENU_ITEMS] = {
    "Manual Run", "Schedules", "Settings", "Status"
};

class MenuSystem {
public:
    MenuSystem();

    // Process an input event. Returns an action if the user completed a flow.
    MenuAction handleInput(InputEvent event);

    // Current screen (for display rendering)
    Screen currentScreen() const { return _screen; }

    // --- Accessors for display rendering ---
    int  selectionIndex() const { return _selIndex; }
    int  cursorPos() const { return _cursorPos; }

    // Schedule editor state
    uint8_t editDaysMask() const { return _editDays; }
    int  editHour() const { return _editHour; }
    int  editMinute() const { return _editMinute; }
    bool editIsPM() const { return _editPM; }
    bool* editZoneEnabled() { return _editZones; }
    const bool* editZoneEnabled() const { return _editZones; }
    int  editDurationMin() const { return _editDuration; }
    int  editZoneId() const;  // current zone being edited for duration
    int  editSlot() const { return _editSlot; }
    bool confirmYes() const { return _confirmYes; }

    // Output data
    ZoneRunQueue& getManualQueue() { return _outputQueue; }
    Schedule& getEditingSchedule() { return _editSchedule; }
    int getActionSlot() const { return _editSlot; }

    // For idle timeout — resets when entering Home
    void goHome();

private:
    void enterScreen(Screen s);
    void buildOutputQueue();
    void buildEditSchedule();
    int  nextSelectedZone(int afterIndex); // find next enabled zone for duration editing
    int  prevSelectedZone(int beforeIndex);
    int  countSelectedZones();

    Screen _screen;
    int    _selIndex;    // for list selection (main menu, schedule list)
    int    _cursorPos;   // for within-screen cursor (days, zones)

    // Schedule editor working state
    int     _editSlot;
    uint8_t _editDays;
    int     _editHour;    // 1-12
    int     _editMinute;  // 0-55 step 5
    bool    _editPM;
    bool    _editZones[4];
    int     _editDuration; // current zone's duration
    int     _durationZoneIndex; // which zone (0-3) we're editing duration for
    int     _durations[4];     // per-zone durations

    bool    _confirmYes;

    // Output
    ZoneRunQueue _outputQueue;
    Schedule     _editSchedule;
};
