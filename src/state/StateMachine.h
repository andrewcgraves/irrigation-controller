#pragma once

#include "../hal/HAL.h"
#include "../clock/ClockManager.h"
#include "../zone/ZoneController.h"
#include "../scheduler/Scheduler.h"
#include "../input/InputManager.h"
#include "../ui/MenuSystem.h"
#include "../network/MQTTManager.h"
#include <cstdint>

enum class AppState : uint8_t {
    Init,
    Idle,
    MenuNav,
    ManualSetup,
    Watering,
    Complete
};

enum class TriggerSource : uint8_t {
    None,
    Schedule,
    MQTT,
    Manual
};

// Callback interface so StateMachine can request display updates
// and network publishes without owning those objects directly
class IStateMachineCallbacks {
public:
    virtual ~IStateMachineCallbacks() = default;

    // Display
    virtual void onShowHome(bool wifiConnected, bool mqttConnected) = 0;
    virtual void onShowMenu(Screen screen, const MenuSystem& menu) = 0;
    virtual void onShowWatering(int zoneId, int elapsedSec, int totalSec, int nextZoneId) = 0;
    virtual void onShowComplete() = 0;
    virtual void onDisplayWake() = 0;
    virtual void onDisplaySleep() = 0;

    // Network
    virtual void onPublishWateringOn(int zoneId, int remainingSec) = 0;
    virtual void onPublishWateringOff() = 0;
    virtual void onPublishZoneState(int zoneId, bool on) = 0;

    // Persistence
    virtual void onSaveSchedules() = 0;
};

class StateMachine {
public:
    StateMachine(IHAL& hal, IClock& clock,
                 ZoneController& zoneCtrl, Scheduler& scheduler,
                 MenuSystem& menu);

    void begin();

    // Call every loop iteration with the current input event and optional MQTT command
    void update(InputEvent input, MQTTCommand* mqttCmd);

    // Set callbacks (display, network, persistence)
    void setCallbacks(IStateMachineCallbacks* cb) { _cb = cb; }

    // Query
    AppState state() const { return _state; }
    TriggerSource triggerSource() const { return _trigger; }

private:
    void transition(AppState newState);
    void handleMQTTCommand(const MQTTCommand& cmd);
    void handleMenuAction(MenuAction action);
    void checkScheduler();
    void updateWateringDisplay();

    IHAL&           _hal;
    IClock&         _clock;
    ZoneController& _zoneCtrl;
    Scheduler&      _scheduler;
    MenuSystem&     _menu;

    IStateMachineCallbacks* _cb;

    AppState      _state;
    TriggerSource _trigger;

    // Idle/display timeout tracking
    unsigned long _lastInputTime;
    bool          _displayOn;

    // Complete screen timer
    unsigned long _completeStartTime;
    static const unsigned long COMPLETE_DISPLAY_MS = 3000;
};
