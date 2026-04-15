#include "StateMachine.h"
#include "../../include/config.h"

StateMachine::StateMachine(IHAL& hal, IClock& clock,
                           ZoneController& zoneCtrl, Scheduler& scheduler,
                           MenuSystem& menu)
    : _hal(hal)
    , _clock(clock)
    , _zoneCtrl(zoneCtrl)
    , _scheduler(scheduler)
    , _menu(menu)
    , _cb(nullptr)
    , _state(AppState::Init)
    , _trigger(TriggerSource::None)
    , _lastInputTime(0)
    , _displayOn(true)
    , _completeStartTime(0)
{}

void StateMachine::begin() {
    _lastInputTime = _hal.millis();
    transition(AppState::Idle);
}

void StateMachine::transition(AppState newState) {
    _state = newState;

    switch (newState) {
        case AppState::Idle:
            _menu.goHome();
            if (_cb) _cb->onShowHome(false, false); // caller updates connectivity
            break;

        case AppState::MenuNav:
        case AppState::ManualSetup:
            if (_cb) _cb->onDisplayWake();
            _displayOn = true;
            break;

        case AppState::Watering:
            if (_cb) _cb->onDisplayWake();
            _displayOn = true;
            break;

        case AppState::Complete:
            _completeStartTime = _hal.millis();
            if (_cb) _cb->onShowComplete();
            if (_cb) _cb->onPublishWateringOff();
            for (int i = 1; i <= ZONE_MAX; i++) {
                if (_cb) _cb->onPublishZoneState(i, false);
            }
            _trigger = TriggerSource::None;
            break;

        default:
            break;
    }
}

void StateMachine::update(InputEvent input, MQTTCommand* mqttCmd) {
    unsigned long now = _hal.millis();

    // --- MQTT commands are always processed (highest priority) ---
    if (mqttCmd && mqttCmd->type != MQTTCommandType::Unknown) {
        handleMQTTCommand(*mqttCmd);
    }

    // --- Input wakes display from any state ---
    if (input != InputEvent::None) {
        _lastInputTime = now;
        if (!_displayOn) {
            if (_cb) _cb->onDisplayWake();
            _displayOn = true;
            // Consume the input that woke the display — don't pass it through
            if (_state == AppState::Idle) {
                return;
            }
        }
    }

    // --- State-specific logic ---
    switch (_state) {

    case AppState::Idle: {
        // Display idle timeout
        if (_displayOn && (now - _lastInputTime >= DISPLAY_IDLE_TIMEOUT_MS)) {
            if (_cb) _cb->onDisplaySleep();
            _displayOn = false;
        }

        // Any input goes to menu
        if (input != InputEvent::None) {
            _menu.goHome();
            MenuAction action = _menu.handleInput(input);
            // After handling, if we're now in MainMenu, transition
            if (_menu.currentScreen() != Screen::Home) {
                transition(AppState::MenuNav);
                if (_cb) _cb->onShowMenu(_menu.currentScreen(), _menu);
            }
        }

        // Check scheduler
        checkScheduler();
        break;
    }

    case AppState::MenuNav: {
        // Menu idle timeout
        if (now - _lastInputTime >= MENU_IDLE_TIMEOUT_MS) {
            transition(AppState::Idle);
            break;
        }

        if (input != InputEvent::None) {
            MenuAction action = _menu.handleInput(input);

            // Check if menu entered manual setup screens
            Screen screen = _menu.currentScreen();
            if (screen == Screen::ManualZoneSelect ||
                screen == Screen::ManualDuration ||
                screen == Screen::ManualConfirm) {
                _state = AppState::ManualSetup;
            }

            handleMenuAction(action);

            if (_state == AppState::MenuNav || _state == AppState::ManualSetup) {
                if (_cb) _cb->onShowMenu(_menu.currentScreen(), _menu);
            }

            // Check if menu returned home
            if (_menu.currentScreen() == Screen::Home) {
                transition(AppState::Idle);
            }
        }

        // Check scheduler even while in menu
        checkScheduler();
        break;
    }

    case AppState::ManualSetup: {
        // Menu idle timeout
        if (now - _lastInputTime >= MENU_IDLE_TIMEOUT_MS) {
            transition(AppState::Idle);
            break;
        }

        if (input != InputEvent::None) {
            MenuAction action = _menu.handleInput(input);
            handleMenuAction(action);

            // Update screen if still in menu
            Screen screen = _menu.currentScreen();
            if (_state == AppState::ManualSetup) {
                // Check if we left manual setup screens
                if (screen != Screen::ManualZoneSelect &&
                    screen != Screen::ManualDuration &&
                    screen != Screen::ManualConfirm) {
                    _state = AppState::MenuNav;
                }
                if (_cb) _cb->onShowMenu(_menu.currentScreen(), _menu);
            }

            if (_menu.currentScreen() == Screen::Home && _state != AppState::Watering) {
                transition(AppState::Idle);
            }
        }

        checkScheduler();
        break;
    }

    case AppState::Watering: {
        // Update zone controller
        _zoneCtrl.update();

        // Update display with progress
        updateWateringDisplay();

        // Check if watering finished
        if (!_zoneCtrl.isWatering()) {
            transition(AppState::Complete);
        }

        // Button press during watering = emergency stop
        if (input == InputEvent::ButtonPress || input == InputEvent::ButtonLongPress) {
            _zoneCtrl.stopAll();
            if (_cb) _cb->onPublishWateringOff();
            for (int i = 1; i <= ZONE_MAX; i++) {
                if (_cb) _cb->onPublishZoneState(i, false);
            }
            _trigger = TriggerSource::None;
            transition(AppState::Idle);
        }
        break;
    }

    case AppState::Complete: {
        // Show "complete" for 3 seconds then go idle
        if (now - _completeStartTime >= COMPLETE_DISPLAY_MS) {
            transition(AppState::Idle);
        }
        break;
    }

    case AppState::Init:
        transition(AppState::Idle);
        break;
    }
}

void StateMachine::handleMQTTCommand(const MQTTCommand& cmd) {
    switch (cmd.type) {
        case MQTTCommandType::TurnOn:
            // MQTT always wins — replace any active watering
            _zoneCtrl.startQueue(const_cast<MQTTCommand&>(cmd).queue);
            _trigger = TriggerSource::MQTT;
            transition(AppState::Watering);
            if (_cb) {
                _cb->onPublishWateringOn(_zoneCtrl.activeZone(),
                                         _zoneCtrl.remainingSeconds());
                _cb->onPublishZoneState(_zoneCtrl.activeZone(), true);
            }
            break;

        case MQTTCommandType::TurnOff:
            _zoneCtrl.stopAll();
            _trigger = TriggerSource::None;
            if (_cb) {
                _cb->onPublishWateringOff();
                for (int i = 1; i <= ZONE_MAX; i++) {
                    _cb->onPublishZoneState(i, false);
                }
            }
            if (_state == AppState::Watering) {
                transition(AppState::Idle);
            }
            break;

        case MQTTCommandType::GetStatus:
            if (_cb) {
                if (_zoneCtrl.isWatering()) {
                    _cb->onPublishWateringOn(_zoneCtrl.activeZone(),
                                             _zoneCtrl.remainingSeconds());
                } else {
                    _cb->onPublishWateringOff();
                }
            }
            break;

        default:
            break;
    }
}

void StateMachine::handleMenuAction(MenuAction action) {
    switch (action) {
        case MenuAction::StartManualWatering: {
            ZoneRunQueue& q = _menu.getManualQueue();
            _zoneCtrl.startQueue(q);
            _trigger = TriggerSource::Manual;
            transition(AppState::Watering);
            if (_cb) {
                _cb->onPublishWateringOn(_zoneCtrl.activeZone(),
                                         _zoneCtrl.remainingSeconds());
                _cb->onPublishZoneState(_zoneCtrl.activeZone(), true);
            }
            break;
        }

        case MenuAction::SaveSchedule: {
            _scheduler.setSchedule(_menu.getActionSlot(), _menu.getEditingSchedule());
            if (_cb) _cb->onSaveSchedules();
            break;
        }

        case MenuAction::DeleteSchedule:
            _scheduler.deleteSchedule(_menu.getActionSlot());
            if (_cb) _cb->onSaveSchedules();
            break;

        case MenuAction::ToggleSchedule:
            _scheduler.toggleSchedule(_menu.getActionSlot());
            if (_cb) _cb->onSaveSchedules();
            break;

        case MenuAction::None:
        default:
            break;
    }
}

void StateMachine::checkScheduler() {
    if (_zoneCtrl.isWatering()) return; // never interrupt active watering

    ZoneRunQueue queue;
    if (_scheduler.checkSchedules(queue)) {
        _zoneCtrl.startQueue(queue);
        _trigger = TriggerSource::Schedule;
        transition(AppState::Watering);
        if (_cb) {
            _cb->onPublishWateringOn(_zoneCtrl.activeZone(),
                                     _zoneCtrl.remainingSeconds());
            _cb->onPublishZoneState(_zoneCtrl.activeZone(), true);
        }
    }
}

void StateMachine::updateWateringDisplay() {
    if (!_cb) return;

    int activeZone = _zoneCtrl.activeZone();
    if (activeZone == 0) return;

    int remainSec  = _zoneCtrl.remainingSeconds();
    int totalSec   = _zoneCtrl.activeDurationSeconds();
    int elapsedSec = totalSec - remainSec;

    _cb->onShowWatering(activeZone, elapsedSec, remainSec, 0);
}
