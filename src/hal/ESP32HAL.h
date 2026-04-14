#pragma once

#ifdef ARDUINO

#include "HAL.h"
#include <Arduino.h>
#include <Preferences.h>

class ESP32HAL : public IHAL {
public:
    ESP32HAL();

    void gpioMode(int pin, int mode) override;
    void gpioWrite(int pin, int value) override;
    int  gpioRead(int pin) override;

    unsigned long millis() override;

    void serialPrint(const char* msg) override;
    void serialPrintln(const char* msg) override;

    bool nvsWrite(const char* key, const uint8_t* data, size_t len) override;
    bool nvsRead(const char* key, uint8_t* data, size_t maxLen, size_t* readLen) override;
    bool nvsErase(const char* key) override;

private:
    Preferences _prefs;
};

#endif // ARDUINO
