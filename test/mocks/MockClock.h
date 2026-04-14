#pragma once

#include "../../src/clock/ClockManager.h"
#include <ctime>

class MockClock : public IClock {
public:
    MockClock() {
        // Default: Sunday Jan 5 2025, 06:00:00
        setTime(2025, 1, 5, 6, 0, 0);
    }

    void setTime(int year, int month, int day, int hr, int min, int sec) {
        struct tm t = {};
        t.tm_year = year - 1900;
        t.tm_mon  = month - 1;
        t.tm_mday = day;
        t.tm_hour = hr;
        t.tm_min  = min;
        t.tm_sec  = sec;
        t.tm_isdst = -1;
        _time = mktime(&t);
    }

    void advanceSeconds(int seconds) {
        _time += seconds;
    }

    void advanceMinutes(int minutes) {
        _time += minutes * 60;
    }

    time_t now() override {
        return _time;
    }

    int hour() override {
        struct tm* t = localtime(&_time);
        return t->tm_hour;
    }

    int minute() override {
        struct tm* t = localtime(&_time);
        return t->tm_min;
    }

    int second() override {
        struct tm* t = localtime(&_time);
        return t->tm_sec;
    }

    int dayOfWeek() override {
        struct tm* t = localtime(&_time);
        return t->tm_wday;
    }

private:
    time_t _time;
};
