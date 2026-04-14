#include "MenuSystem.h"
#include "../../include/config.h"

MenuSystem::MenuSystem()
    : _screen(Screen::Home)
    , _selIndex(0)
    , _cursorPos(0)
    , _editSlot(-1)
    , _editDays(0)
    , _editHour(6)
    , _editMinute(0)
    , _editPM(false)
    , _editDuration(DEFAULT_WATERING_DURATION_MIN)
    , _durationZoneIndex(-1)
    , _confirmYes(true)
{
    for (int i = 0; i < 4; i++) {
        _editZones[i] = false;
        _durations[i] = DEFAULT_WATERING_DURATION_MIN;
    }
}

void MenuSystem::goHome() {
    _screen = Screen::Home;
    _selIndex = 0;
    _cursorPos = 0;
}

void MenuSystem::enterScreen(Screen s) {
    _screen = s;
    _cursorPos = 0;
}

int MenuSystem::editZoneId() const {
    if (_durationZoneIndex >= 0 && _durationZoneIndex < 4) {
        return _durationZoneIndex + 1;
    }
    return 0;
}

int MenuSystem::nextSelectedZone(int afterIndex) {
    for (int i = afterIndex; i < 4; i++) {
        if (_editZones[i]) return i;
    }
    return -1;
}

int MenuSystem::prevSelectedZone(int beforeIndex) {
    for (int i = beforeIndex - 1; i >= 0; i--) {
        if (_editZones[i]) return i;
    }
    return -1;
}

int MenuSystem::countSelectedZones() {
    int c = 0;
    for (int i = 0; i < 4; i++) {
        if (_editZones[i]) c++;
    }
    return c;
}

void MenuSystem::buildOutputQueue() {
    _outputQueue.clear();
    for (int i = 0; i < 4; i++) {
        if (_editZones[i]) {
            _outputQueue.add(static_cast<uint8_t>(i + 1),
                             static_cast<uint16_t>(_durations[i]));
        }
    }
}

void MenuSystem::buildEditSchedule() {
    _editSchedule.clear();
    _editSchedule.enabled = true;
    _editSchedule.daysOfWeek = _editDays;

    // Convert 12h to 24h
    int h24 = _editHour % 12;
    if (_editPM) h24 += 12;
    _editSchedule.startHour = h24;
    _editSchedule.startMinute = _editMinute;

    for (int i = 0; i < 4; i++) {
        if (_editZones[i]) {
            _editSchedule.zones[_editSchedule.zoneCount] = {
                static_cast<uint8_t>(i + 1),
                static_cast<uint16_t>(_durations[i])
            };
            _editSchedule.zoneCount++;
        }
    }
}

// ============================================================
// Input handling — returns MenuAction when a flow completes
// ============================================================

MenuAction MenuSystem::handleInput(InputEvent event) {
    if (event == InputEvent::None) return MenuAction::None;

    switch (_screen) {

    // ---- Home Screen ----
    case Screen::Home:
        if (event == InputEvent::ButtonPress ||
            event == InputEvent::RotateCW ||
            event == InputEvent::RotateCCW) {
            _screen = Screen::MainMenu;
            _selIndex = 0;
        }
        return MenuAction::None;

    // ---- Main Menu ----
    case Screen::MainMenu:
        if (event == InputEvent::RotateCW) {
            _selIndex = (_selIndex + 1) % MAIN_MENU_ITEMS;
        } else if (event == InputEvent::RotateCCW) {
            _selIndex = (_selIndex - 1 + MAIN_MENU_ITEMS) % MAIN_MENU_ITEMS;
        } else if (event == InputEvent::ButtonPress) {
            switch (_selIndex) {
                case 0: // Manual Run
                    for (int i = 0; i < 4; i++) {
                        _editZones[i] = false;
                        _durations[i] = DEFAULT_WATERING_DURATION_MIN;
                    }
                    enterScreen(Screen::ManualZoneSelect);
                    break;
                case 1: // Schedules
                    _selIndex = 0;
                    enterScreen(Screen::ScheduleList);
                    break;
                case 2: // Settings
                    enterScreen(Screen::Settings);
                    break;
                case 3: // Status
                    enterScreen(Screen::Status);
                    break;
            }
        } else if (event == InputEvent::ButtonLongPress) {
            goHome();
        }
        return MenuAction::None;

    // ---- Manual Zone Select ----
    case Screen::ManualZoneSelect:
        if (event == InputEvent::RotateCW) {
            _cursorPos = (_cursorPos + 1) % 4;
        } else if (event == InputEvent::RotateCCW) {
            _cursorPos = (_cursorPos + 3) % 4;
        } else if (event == InputEvent::ButtonPress) {
            _editZones[_cursorPos] = !_editZones[_cursorPos];
        } else if (event == InputEvent::ButtonLongPress) {
            if (countSelectedZones() > 0) {
                _durationZoneIndex = nextSelectedZone(0);
                _editDuration = _durations[_durationZoneIndex];
                enterScreen(Screen::ManualDuration);
            } else {
                _screen = Screen::MainMenu;
                _selIndex = 0;
            }
        }
        return MenuAction::None;

    // ---- Manual Duration (per zone) ----
    case Screen::ManualDuration:
        if (event == InputEvent::RotateCW) {
            _editDuration++;
            if (_editDuration > MAX_WATERING_DURATION_MIN) _editDuration = MAX_WATERING_DURATION_MIN;
        } else if (event == InputEvent::RotateCCW) {
            _editDuration--;
            if (_editDuration < 1) _editDuration = 1;
        } else if (event == InputEvent::ButtonPress) {
            // Save this zone's duration and advance to next
            _durations[_durationZoneIndex] = _editDuration;
            int next = nextSelectedZone(_durationZoneIndex + 1);
            if (next >= 0) {
                _durationZoneIndex = next;
                _editDuration = _durations[_durationZoneIndex];
            } else {
                // All zones configured — confirm
                _confirmYes = true;
                enterScreen(Screen::ManualConfirm);
            }
        } else if (event == InputEvent::ButtonLongPress) {
            // Go back to previous zone or zone select
            int prev = prevSelectedZone(_durationZoneIndex);
            if (prev >= 0) {
                _durationZoneIndex = prev;
                _editDuration = _durations[_durationZoneIndex];
            } else {
                enterScreen(Screen::ManualZoneSelect);
            }
        }
        return MenuAction::None;

    // ---- Manual Confirm ----
    case Screen::ManualConfirm:
        if (event == InputEvent::RotateCW || event == InputEvent::RotateCCW) {
            _confirmYes = !_confirmYes;
        } else if (event == InputEvent::ButtonPress) {
            if (_confirmYes) {
                buildOutputQueue();
                goHome();
                return MenuAction::StartManualWatering;
            } else {
                goHome();
            }
        } else if (event == InputEvent::ButtonLongPress) {
            // Back to last duration
            _durationZoneIndex = prevSelectedZone(4); // last selected
            if (_durationZoneIndex >= 0) {
                _editDuration = _durations[_durationZoneIndex];
                enterScreen(Screen::ManualDuration);
            } else {
                enterScreen(Screen::ManualZoneSelect);
            }
        }
        return MenuAction::None;

    // ---- Schedule List ----
    case Screen::ScheduleList:
        if (event == InputEvent::RotateCW) {
            _selIndex = (_selIndex + 1) % SCHEDULER_MAX_SLOTS;
        } else if (event == InputEvent::RotateCCW) {
            _selIndex = (_selIndex - 1 + SCHEDULER_MAX_SLOTS) % SCHEDULER_MAX_SLOTS;
        } else if (event == InputEvent::ButtonPress) {
            _editSlot = _selIndex;
            _cursorPos = 0; // Edit
            enterScreen(Screen::ScheduleAction);
        } else if (event == InputEvent::ButtonLongPress) {
            _screen = Screen::MainMenu;
            _selIndex = 1; // back to Schedules in main menu
        }
        return MenuAction::None;

    // ---- Schedule Action (Edit / On/Off / Delete) ----
    case Screen::ScheduleAction:
        if (event == InputEvent::RotateCW) {
            _cursorPos = (_cursorPos + 1) % 3;
        } else if (event == InputEvent::RotateCCW) {
            _cursorPos = (_cursorPos + 2) % 3;
        } else if (event == InputEvent::ButtonPress) {
            if (_cursorPos == 0) {
                // Edit — initialize editor state
                _editDays = 0;
                _editHour = 6;
                _editMinute = 0;
                _editPM = false;
                for (int i = 0; i < 4; i++) {
                    _editZones[i] = false;
                    _durations[i] = DEFAULT_WATERING_DURATION_MIN;
                }
                enterScreen(Screen::EditDays);
            } else if (_cursorPos == 1) {
                // Toggle on/off
                enterScreen(Screen::ScheduleList);
                return MenuAction::ToggleSchedule;
            } else {
                // Delete
                enterScreen(Screen::ScheduleList);
                return MenuAction::DeleteSchedule;
            }
        } else if (event == InputEvent::ButtonLongPress) {
            enterScreen(Screen::ScheduleList);
        }
        return MenuAction::None;

    // ---- Edit Days ----
    case Screen::EditDays:
        if (event == InputEvent::RotateCW) {
            _cursorPos = (_cursorPos + 1) % 7;
        } else if (event == InputEvent::RotateCCW) {
            _cursorPos = (_cursorPos + 6) % 7;
        } else if (event == InputEvent::ButtonPress) {
            _editDays ^= (1 << _cursorPos); // toggle day
        } else if (event == InputEvent::ButtonLongPress) {
            if (_editDays != 0) {
                enterScreen(Screen::EditTimeHour);
            } else {
                // No days selected — go back to schedule list
                enterScreen(Screen::ScheduleList);
            }
        }
        return MenuAction::None;

    // ---- Edit Time Hour ----
    case Screen::EditTimeHour:
        if (event == InputEvent::RotateCW) {
            _editHour++;
            if (_editHour > 12) _editHour = 1;
        } else if (event == InputEvent::RotateCCW) {
            _editHour--;
            if (_editHour < 1) _editHour = 12;
        } else if (event == InputEvent::ButtonPress) {
            enterScreen(Screen::EditTimeMinute);
        } else if (event == InputEvent::ButtonLongPress) {
            enterScreen(Screen::EditDays);
        }
        return MenuAction::None;

    // ---- Edit Time Minute ----
    case Screen::EditTimeMinute:
        if (event == InputEvent::RotateCW) {
            _editMinute += 5;
            if (_editMinute >= 60) _editMinute = 0;
        } else if (event == InputEvent::RotateCCW) {
            _editMinute -= 5;
            if (_editMinute < 0) _editMinute = 55;
        } else if (event == InputEvent::ButtonPress) {
            enterScreen(Screen::EditTimeAMPM);
        } else if (event == InputEvent::ButtonLongPress) {
            enterScreen(Screen::EditTimeHour);
        }
        return MenuAction::None;

    // ---- Edit Time AM/PM ----
    case Screen::EditTimeAMPM:
        if (event == InputEvent::RotateCW || event == InputEvent::RotateCCW) {
            _editPM = !_editPM;
        } else if (event == InputEvent::ButtonPress) {
            enterScreen(Screen::EditZoneSelect);
        } else if (event == InputEvent::ButtonLongPress) {
            enterScreen(Screen::EditTimeMinute);
        }
        return MenuAction::None;

    // ---- Edit Zone Select ----
    case Screen::EditZoneSelect:
        if (event == InputEvent::RotateCW) {
            _cursorPos = (_cursorPos + 1) % 4;
        } else if (event == InputEvent::RotateCCW) {
            _cursorPos = (_cursorPos + 3) % 4;
        } else if (event == InputEvent::ButtonPress) {
            _editZones[_cursorPos] = !_editZones[_cursorPos];
        } else if (event == InputEvent::ButtonLongPress) {
            if (countSelectedZones() > 0) {
                _durationZoneIndex = nextSelectedZone(0);
                _editDuration = _durations[_durationZoneIndex];
                enterScreen(Screen::EditDuration);
            } else {
                enterScreen(Screen::EditTimeAMPM);
            }
        }
        return MenuAction::None;

    // ---- Edit Duration (per zone) ----
    case Screen::EditDuration:
        if (event == InputEvent::RotateCW) {
            _editDuration++;
            if (_editDuration > MAX_WATERING_DURATION_MIN) _editDuration = MAX_WATERING_DURATION_MIN;
        } else if (event == InputEvent::RotateCCW) {
            _editDuration--;
            if (_editDuration < 1) _editDuration = 1;
        } else if (event == InputEvent::ButtonPress) {
            _durations[_durationZoneIndex] = _editDuration;
            int next = nextSelectedZone(_durationZoneIndex + 1);
            if (next >= 0) {
                _durationZoneIndex = next;
                _editDuration = _durations[_durationZoneIndex];
            } else {
                _confirmYes = true;
                enterScreen(Screen::EditConfirm);
            }
        } else if (event == InputEvent::ButtonLongPress) {
            int prev = prevSelectedZone(_durationZoneIndex);
            if (prev >= 0) {
                _durationZoneIndex = prev;
                _editDuration = _durations[_durationZoneIndex];
            } else {
                enterScreen(Screen::EditZoneSelect);
            }
        }
        return MenuAction::None;

    // ---- Edit Confirm ----
    case Screen::EditConfirm:
        if (event == InputEvent::RotateCW || event == InputEvent::RotateCCW) {
            _confirmYes = !_confirmYes;
        } else if (event == InputEvent::ButtonPress) {
            if (_confirmYes) {
                buildEditSchedule();
                enterScreen(Screen::ScheduleList);
                return MenuAction::SaveSchedule;
            } else {
                enterScreen(Screen::ScheduleList);
            }
        } else if (event == InputEvent::ButtonLongPress) {
            _durationZoneIndex = prevSelectedZone(4);
            if (_durationZoneIndex >= 0) {
                _editDuration = _durations[_durationZoneIndex];
                enterScreen(Screen::EditDuration);
            } else {
                enterScreen(Screen::EditZoneSelect);
            }
        }
        return MenuAction::None;

    // ---- Settings ----
    case Screen::Settings:
        if (event == InputEvent::ButtonLongPress || event == InputEvent::ButtonPress) {
            _screen = Screen::MainMenu;
            _selIndex = 2;
        }
        return MenuAction::None;

    // ---- Status ----
    case Screen::Status:
        if (event == InputEvent::ButtonLongPress || event == InputEvent::ButtonPress) {
            _screen = Screen::MainMenu;
            _selIndex = 3;
        }
        return MenuAction::None;
    }

    return MenuAction::None;
}
