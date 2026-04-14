#ifdef ARDUINO

#include "ESP32HAL.h"

static const char* NVS_NAMESPACE = "irrigation";

ESP32HAL::ESP32HAL() {
    _prefs.begin(NVS_NAMESPACE, false);
}

void ESP32HAL::gpioMode(int pin, int mode) {
    pinMode(pin, mode);
}

void ESP32HAL::gpioWrite(int pin, int value) {
    digitalWrite(pin, value);
}

int ESP32HAL::gpioRead(int pin) {
    return digitalRead(pin);
}

unsigned long ESP32HAL::millis() {
    return ::millis();
}

void ESP32HAL::serialPrint(const char* msg) {
    Serial.print(msg);
}

void ESP32HAL::serialPrintln(const char* msg) {
    Serial.println(msg);
}

bool ESP32HAL::nvsWrite(const char* key, const uint8_t* data, size_t len) {
    return _prefs.putBytes(key, data, len) == len;
}

bool ESP32HAL::nvsRead(const char* key, uint8_t* data, size_t maxLen, size_t* readLen) {
    size_t stored = _prefs.getBytesLength(key);
    if (stored == 0 || stored > maxLen) {
        if (readLen) *readLen = 0;
        return false;
    }
    size_t got = _prefs.getBytes(key, data, maxLen);
    if (readLen) *readLen = got;
    return got > 0;
}

bool ESP32HAL::nvsErase(const char* key) {
    return _prefs.remove(key);
}

#endif // ARDUINO
