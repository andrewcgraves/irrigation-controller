#ifdef ARDUINO

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "secrets.h"
#include "hal/ESP32HAL.h"
#include "clock/ClockManager.h"
#include "zone/ZoneController.h"
#include "scheduler/Scheduler.h"
#include "network/MQTTManager.h"
#include "display/DisplayManager.h"
#include "input/InputManager.h"
#include "ui/MenuSystem.h"
#include "state/StateMachine.h"
#include "led/LEDManager.h"

// Watchdog timeout — reset if loop stalls for 30 seconds
static const int WDT_TIMEOUT_SEC = 30;

// --- Global instances ---
static ESP32HAL hal;
static ClockManager clockMgr;
static ZoneController zoneCtrl(hal, clockMgr, ZONE_RELAY_PINS, NUM_ZONES);
static Scheduler scheduler(hal, clockMgr);
static MQTTManager mqtt(hal);
static DisplayManager display(hal, clockMgr,
                              PIN_OLED_MOSI, PIN_OLED_CLK,
                              PIN_OLED_DC, PIN_OLED_RST, PIN_OLED_CS);
static InputManager input(hal, PIN_ENCODER_CLK, PIN_ENCODER_DT, PIN_ENCODER_SW);
static MenuSystem menu;
static StateMachine sm(hal, clockMgr, zoneCtrl, scheduler, menu);
static LEDManager leds(hal, ZONE_LED_PINS, NUM_ZONES);

// --- Callback bridge: StateMachine → Display / MQTT / NVS ---
class AppCallbacks : public IStateMachineCallbacks {
public:
    void onShowHome(bool wifiConnected, bool mqttConnected) override {
        display.showHomeScreen(wifiConnected, mqttConnected, nullptr);
    }

    void onShowMenu(Screen screen, const MenuSystem& m) override {
        // Render the current menu screen on the OLED
        switch (screen) {
            case Screen::MainMenu: {
                static const char* menuItems[] = {"Manual Run", "Schedules", "Settings", "Status"};
                int sel = m.selectionIndex();
                display.showMenuItem("Menu", menuItems[sel]);
                break;
            }
            case Screen::ManualZoneSelect:
                display.showZoneSelector("Select Zones",
                    m.editZoneEnabled(), m.cursorPos());
                break;
            case Screen::ManualDuration:
                display.showDurationEditor(m.editZoneId(), m.editDurationMin());
                break;
            case Screen::ManualConfirm:
                display.showConfirm("Start watering?", m.confirmYes());
                break;
            case Screen::ScheduleList: {
                char buf[20];
                snprintf(buf, sizeof(buf), "Slot %d", m.selectionIndex() + 1);
                display.showMenuItem("Schedules", buf);
                break;
            }
            case Screen::ScheduleAction: {
                static const char* actions[] = {"Edit", "On/Off", "Delete"};
                display.showMenuItem("Action", actions[m.cursorPos()]);
                break;
            }
            case Screen::EditDays:
                display.showDaySelector("Days", m.editDaysMask(), m.cursorPos());
                break;
            case Screen::EditTimeHour:
                display.showTimeEditor("Start Time", m.editHour(), m.editMinute(),
                                       m.editIsPM(), 0);
                break;
            case Screen::EditTimeMinute:
                display.showTimeEditor("Start Time", m.editHour(), m.editMinute(),
                                       m.editIsPM(), 1);
                break;
            case Screen::EditTimeAMPM:
                display.showTimeEditor("Start Time", m.editHour(), m.editMinute(),
                                       m.editIsPM(), 2);
                break;
            case Screen::EditZoneSelect:
                display.showZoneSelector("Sched Zones",
                    m.editZoneEnabled(), m.cursorPos());
                break;
            case Screen::EditDuration:
                display.showDurationEditor(m.editZoneId(), m.editDurationMin());
                break;
            case Screen::EditConfirm:
                display.showConfirm("Save schedule?", m.confirmYes());
                break;
            case Screen::Settings:
                display.showMessage("Settings", "Press to go back");
                break;
            case Screen::Status: {
                char uptime[32];
                unsigned long sec = hal.millis() / 1000;
                snprintf(uptime, sizeof(uptime), "Up %luh %lum", sec / 3600, (sec % 3600) / 60);
                display.showMessage("Status", uptime);
                break;
            }
            default:
                break;
        }
    }

    void onShowWatering(int zoneId, int elapsedSec, int totalSec, int nextZoneId) override {
        display.showWateringProgress(zoneId, elapsedSec, totalSec, nextZoneId);
    }

    void onShowComplete() override {
        display.showMessage("Complete!", "All zones done");
    }

    void onDisplayWake() override {
        display.wake();
    }

    void onDisplaySleep() override {
        // Display manages its own sleep via checkIdleTimeout, but we can
        // explicitly turn off here if the state machine requests it.
        display.showMessage("", ""); // blank the screen
    }

    void onPublishWateringOn(int zoneId, int remainingSec) override {
        mqtt.publishWateringStatus(true, zoneId, remainingSec);
    }

    void onPublishWateringOff() override {
        mqtt.publishWateringStatus(false);
    }

    void onPublishZoneState(int zoneId, bool on) override {
        mqtt.publishZoneState(zoneId, on);
    }

    void onSaveSchedules() override {
        scheduler.saveToNVS();
    }
};

static AppCallbacks callbacks;

void setup() {
    Serial.begin(115200);
    delay(500); // brief delay for serial monitor

    Serial.println("Irrigation Controller starting...");

    // Initialize hardware
    zoneCtrl.begin();
    leds.begin();
    input.begin();
    display.begin();

    // Load saved schedules from NVS
    scheduler.loadFromNVS();

    // Start WiFi + MQTT
    mqtt.begin(SECRET_SSID, SECRET_PASS,
               SECRET_BROKER_IP, SECRET_BROKER_PORT,
               SECRET_BROKER_NAME, SECRET_BROKER_USERNAME, SECRET_BROKER_PASSWORD);

    // NTP time sync (UTC-7 = -25200 seconds, no DST)
    clockMgr.beginNTP(-25200, 0);

    // Wire up state machine callbacks and start
    sm.setCallbacks(&callbacks);
    sm.begin();

    // OTA updates — allows flashing over WiFi
    ArduinoOTA.setHostname("irrigation-controller");
    ArduinoOTA.onStart([]() {
        // Stop watering during OTA to be safe
        zoneCtrl.stopAll();
        display.showMessage("OTA Update", "Flashing...");
    });
    ArduinoOTA.onEnd([]() {
        display.showMessage("OTA Update", "Done! Rebooting");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        display.showMessage("OTA Error", "Update failed");
    });
    ArduinoOTA.begin();

    // Watchdog timer — resets MCU if loop() stalls
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true); // true = trigger reset on timeout
    esp_task_wdt_add(NULL); // watch the current (main) task

    Serial.println("Setup complete.");
}

void loop() {
    // Feed watchdog — must be called every iteration
    esp_task_wdt_reset();

    // Handle OTA updates (non-blocking check)
    ArduinoOTA.handle();

    // Update network (non-blocking WiFi/MQTT reconnect)
    mqtt.update();

    // Poll encoder + button for input events
    InputEvent event = input.poll();

    // Check for pending MQTT commands
    MQTTCommand* mqttCmd = nullptr;
    MQTTCommand cmd;
    if (mqtt.hasCommand()) {
        cmd = mqtt.consumeCommand();
        mqttCmd = &cmd;
    }

    // Run state machine (handles all transitions, display, scheduling)
    sm.update(event, mqttCmd);

    // Update per-zone LEDs (blink active, solid completed, off idle)
    leds.update(zoneCtrl);
}

#endif // ARDUINO
