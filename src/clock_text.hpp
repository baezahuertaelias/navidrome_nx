#pragma once
#include <switch.h>
#include <cstdio>
#include <string>
#include <chrono>

inline std::string clockText() {
    static long long lastSecond = -1;
    static std::string cached = "--:--";
    const auto second = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (second == lastSecond) return cached;
    lastSecond = second;
    u64 now;
    TimeCalendarTime calendar;
    TimeCalendarAdditionalInfo info;
    if (R_FAILED(timeGetCurrentTime(TimeType_LocalSystemClock, &now))
        || R_FAILED(timeToCalendarTimeWithMyRule(now, &calendar, &info))) return cached = "--:--";
    char text[16];
    std::snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(calendar.hour), static_cast<unsigned>(calendar.minute));
    return cached = text;
}
