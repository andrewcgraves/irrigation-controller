#include "DisplayManager.h"
#include "../../include/config.h"
#include <cstdio>

#ifdef ARDUINO

DisplayManager::DisplayManager(IHAL& hal, IClock& clock,
                               int mosiPin, int clkPin, int dcPin, int rstPin, int csPin)
    : _hal(hal)
    , _clock(clock)
    , _displayOn(false)
    , _lastWakeTime(0)
    , _mosiPin(mosiPin)
    , _clkPin(clkPin)
    , _dcPin(dcPin)
    , _rstPin(rstPin)
    , _csPin(csPin)
{
    // Hardware SPI constructor: pass &SPI, dc, rst, cs
    _oled = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &SPI, dcPin, rstPin, csPin);
}

void DisplayManager::begin() {
    // Explicitly init the SPI bus with the correct pins for the Nano ESP32
    // (SCK, MISO=-1, MOSI, SS)
    SPI.begin(_clkPin, -1, _mosiPin, _csPin);

    if (!_oled->begin(SSD1306_SWITCHCAPVCC, 0)) {
        _hal.serialPrintln("SSD1306 init failed");
        return;
    }

    // Maximise contrast so pixels are as bright as possible
    _oled->ssd1306_command(SSD1306_SETCONTRAST);
    _oled->ssd1306_command(0xFF);

    // Test pattern — fills screen white so we can confirm hardware is working
    _oled->clearDisplay();
    _oled->fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
    _oled->display();
    delay(2000); // Hold for 2 seconds so you can see it

    _oled->clearDisplay();
    _oled->setTextSize(1);
    _oled->setTextColor(SSD1306_WHITE);
    _oled->setCursor(0, 0);
    _oled->println("Init OK");
    _oled->display();
    delay(1000);

    _oled->clearDisplay();
    _oled->display();
    _displayOn = true;
    _lastWakeTime = _hal.millis();
}

void DisplayManager::wake() {
    if (!_displayOn) {
        _oled->ssd1306_command(SSD1306_DISPLAYON);
        _displayOn = true;
    }
    _lastWakeTime = _hal.millis();
}

void DisplayManager::checkIdleTimeout() {
    if (_displayOn && (_hal.millis() - _lastWakeTime >= DISPLAY_IDLE_TIMEOUT_MS)) {
        _oled->ssd1306_command(SSD1306_DISPLAYOFF);
        _displayOn = false;
    }
}

void DisplayManager::clearAndDisplay() {
    _oled->display();
}

void DisplayManager::showHomeScreen(bool wifiConnected, bool mqttConnected,
                                     const char* nextScheduleText) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);

    // Line 1: Time + connection status
    char timeBuf[20];
    int h = _clock.hour();
    bool pm = h >= 12;
    int h12 = h % 12;
    if (h12 == 0) h12 = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d %s  %s%s",
             h12, _clock.minute(), pm ? "PM" : "AM",
             wifiConnected ? "W" : "-",
             mqttConnected ? "M" : "-");
    _oled->println(timeBuf);

    // Line 2: Next schedule
    if (nextScheduleText && nextScheduleText[0]) {
        char buf[22];
        snprintf(buf, sizeof(buf), "Next: %s", nextScheduleText);
        _oled->println(buf);
    } else {
        _oled->println("No schedules");
    }

    clearAndDisplay();
}

void DisplayManager::showMenuItem(const char* title, const char* value) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);
    _oled->println(title);
    _oled->print("> ");
    _oled->println(value);
    clearAndDisplay();
}

void DisplayManager::showWateringProgress(int zoneId, int elapsedSec, int totalSec,
                                           int nextZoneId) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);

    // Line 1: Zone info
    char buf[22];
    int remainMin = (totalSec - elapsedSec) / 60;
    int remainSec = (totalSec - elapsedSec) % 60;
    snprintf(buf, sizeof(buf), "Zone %d  %d:%02d left", zoneId, remainMin, remainSec);
    _oled->println(buf);

    // Line 2: Progress bar
    int barWidth = 100;
    int filled = (totalSec > 0) ? (elapsedSec * barWidth / totalSec) : 0;
    if (filled > barWidth) filled = barWidth;

    _oled->drawRect(0, 18, barWidth + 2, 10, SSD1306_WHITE);
    _oled->fillRect(1, 19, filled, 8, SSD1306_WHITE);

    // Next zone indicator
    if (nextZoneId > 0) {
        char nextBuf[8];
        snprintf(nextBuf, sizeof(nextBuf), ">Z%d", nextZoneId);
        _oled->setCursor(104, 18);
        _oled->print(nextBuf);
    }

    clearAndDisplay();
}

void DisplayManager::showDaySelector(const char* title, uint8_t daysMask, int cursorPos) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);
    _oled->println(title);

    const char* dayLabels[] = {"S", "M", "T", "W", "T", "F", "S"};
    char line[22] = "";
    int pos = 0;

    for (int d = 0; d < 7; d++) {
        bool selected = daysMask & (1 << d);
        if (d == cursorPos) {
            pos += snprintf(line + pos, sizeof(line) - pos, "[%s]", dayLabels[d]);
        } else if (selected) {
            pos += snprintf(line + pos, sizeof(line) - pos, " %s ", dayLabels[d]);
        } else {
            pos += snprintf(line + pos, sizeof(line) - pos, " . ");
        }
    }
    _oled->println(line);
    clearAndDisplay();
}

void DisplayManager::showTimeEditor(const char* title, int hour, int minute, bool isPM,
                                     int cursorField) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);
    _oled->println(title);

    char line[22];
    // Show cursor brackets around active field
    if (cursorField == 0) {
        snprintf(line, sizeof(line), "[%2d]:%02d %s", hour, minute, isPM ? "PM" : "AM");
    } else if (cursorField == 1) {
        snprintf(line, sizeof(line), " %2d:[%02d] %s", hour, minute, isPM ? "PM" : "AM");
    } else {
        snprintf(line, sizeof(line), " %2d:%02d [%s]", hour, minute, isPM ? "PM" : "AM");
    }
    _oled->println(line);
    clearAndDisplay();
}

void DisplayManager::showZoneSelector(const char* title, const bool zoneEnabled[4], int cursorPos) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);
    _oled->println(title);

    char line[22] = "";
    int pos = 0;
    for (int z = 0; z < 4; z++) {
        const char* mark = zoneEnabled[z] ? "+" : "-";
        if (z == cursorPos) {
            pos += snprintf(line + pos, sizeof(line) - pos, "[Z%d:%s]", z + 1, mark);
        } else {
            pos += snprintf(line + pos, sizeof(line) - pos, " Z%d:%s ", z + 1, mark);
        }
    }
    _oled->println(line);
    clearAndDisplay();
}

void DisplayManager::showDurationEditor(int zoneId, int durationMin) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);

    char title[22];
    snprintf(title, sizeof(title), "Zone %d Duration", zoneId);
    _oled->println(title);

    char value[22];
    snprintf(value, sizeof(value), "> %d min", durationMin);
    _oled->println(value);

    clearAndDisplay();
}

void DisplayManager::showConfirm(const char* question, bool yesSelected) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);
    _oled->println(question);

    if (yesSelected) {
        _oled->println("[Yes]     No");
    } else {
        _oled->println(" Yes     [No]");
    }
    clearAndDisplay();
}

void DisplayManager::showMessage(const char* line1, const char* line2) {
    _oled->clearDisplay();
    _oled->setCursor(0, 0);
    _oled->println(line1);
    if (line2) {
        _oled->println(line2);
    }
    clearAndDisplay();
}

#else
// Native stub for compilation in tests — display is not tested on desktop
DisplayManager::DisplayManager(IHAL& hal, IClock& clock)
    : _hal(hal), _clock(clock), _displayOn(false), _lastWakeTime(0) {}

void DisplayManager::begin() {}
void DisplayManager::wake() { _displayOn = true; _lastWakeTime = _hal.millis(); }
void DisplayManager::checkIdleTimeout() {
    if (_displayOn && (_hal.millis() - _lastWakeTime >= DISPLAY_IDLE_TIMEOUT_MS)) {
        _displayOn = false;
    }
}
void DisplayManager::showHomeScreen(bool, bool, const char*) {}
void DisplayManager::showMenuItem(const char*, const char*) {}
void DisplayManager::showWateringProgress(int, int, int, int) {}
void DisplayManager::showDaySelector(const char*, uint8_t, int) {}
void DisplayManager::showTimeEditor(const char*, int, int, bool, int) {}
void DisplayManager::showZoneSelector(const char*, const bool[4], int) {}
void DisplayManager::showDurationEditor(int, int) {}
void DisplayManager::showConfirm(const char*, bool) {}
void DisplayManager::showMessage(const char*, const char*) {}

#endif // ARDUINO
