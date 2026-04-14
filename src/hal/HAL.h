#pragma once

#include <cstdint>
#include <cstddef>

class IHAL {
public:
    virtual ~IHAL() = default;

    // Digital I/O (named gpio* to avoid Arduino macro collisions)
    virtual void gpioMode(int pin, int mode) = 0;
    virtual void gpioWrite(int pin, int value) = 0;
    virtual int  gpioRead(int pin) = 0;

    // Timing
    virtual unsigned long millis() = 0;

    // Serial output
    virtual void serialPrint(const char* msg) = 0;
    virtual void serialPrintln(const char* msg) = 0;

    // NVS (non-volatile storage)
    virtual bool nvsWrite(const char* key, const uint8_t* data, size_t len) = 0;
    virtual bool nvsRead(const char* key, uint8_t* data, size_t maxLen, size_t* readLen) = 0;
    virtual bool nvsErase(const char* key) = 0;
};

// Pin mode / logic level constants.
// On Arduino/ESP32 these are provided by framework headers and may differ
// (e.g. INPUT=0x01, OUTPUT=0x03). We use matching values so there is no
// redefinition warning when both headers are included in either order.
#ifndef INPUT
  #define INPUT  0x01
#endif
#ifndef OUTPUT
  #define OUTPUT 0x03
#endif
#ifndef INPUT_PULLUP
  #define INPUT_PULLUP 0x05
#endif
#ifndef HIGH
  #define HIGH 0x1
#endif
#ifndef LOW
  #define LOW  0x0
#endif
