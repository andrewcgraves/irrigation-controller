#pragma once

#include "../../src/hal/HAL.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>

class MockHAL : public IHAL {
public:
    // --- Digital I/O tracking ---
    struct PinConfig {
        int mode = -1;
        int value = HIGH;
    };

    std::map<int, PinConfig> pins;

    void gpioMode(int pin, int mode) override {
        pins[pin].mode = mode;
    }

    void gpioWrite(int pin, int value) override {
        pins[pin].value = value;
        writeLog.push_back({pin, value});
    }

    int gpioRead(int pin) override {
        auto it = pins.find(pin);
        if (it != pins.end()) return it->second.value;
        return HIGH;
    }

    // Track all writes for assertion
    struct WriteEntry {
        int pin;
        int value;
    };
    std::vector<WriteEntry> writeLog;

    // --- Timing ---
    unsigned long _millis = 0;

    unsigned long millis() override {
        return _millis;
    }

    void advanceMillis(unsigned long ms) {
        _millis += ms;
    }

    // --- Serial output ---
    std::string serialOutput;

    void serialPrint(const char* msg) override {
        serialOutput += msg;
    }

    void serialPrintln(const char* msg) override {
        serialOutput += msg;
        serialOutput += "\n";
    }

    // --- NVS mock (in-memory key-value store) ---
    struct NvsEntry {
        std::vector<uint8_t> data;
    };
    std::map<std::string, NvsEntry> nvsStore;

    bool nvsWrite(const char* key, const uint8_t* data, size_t len) override {
        nvsStore[key].data.assign(data, data + len);
        return true;
    }

    bool nvsRead(const char* key, uint8_t* data, size_t maxLen, size_t* readLen) override {
        auto it = nvsStore.find(key);
        if (it == nvsStore.end() || it->second.data.size() > maxLen) {
            if (readLen) *readLen = 0;
            return false;
        }
        std::memcpy(data, it->second.data.data(), it->second.data.size());
        if (readLen) *readLen = it->second.data.size();
        return true;
    }

    bool nvsErase(const char* key) override {
        return nvsStore.erase(key) > 0;
    }

    // --- Helpers for tests ---
    int pinValue(int pin) const {
        auto it = pins.find(pin);
        if (it != pins.end()) return it->second.value;
        return HIGH;
    }

    void reset() {
        pins.clear();
        writeLog.clear();
        serialOutput.clear();
        nvsStore.clear();
        _millis = 0;
    }
};
