#pragma once
#ifdef __SWITCH__
#include <switch.h>
#else
#include <ctime>
#endif
#include <cstdio>
#include <string>
#include <chrono>

inline std::string clockText() {
    static long long lastSecond = -1;
    static std::string cached = "--:--";
    const auto second = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (second == lastSecond) return cached;
    lastSecond = second;
    char text[16];
#ifdef __SWITCH__
    u64 now;
    TimeCalendarTime calendar;
    TimeCalendarAdditionalInfo info;
    if (R_FAILED(timeGetCurrentTime(TimeType_LocalSystemClock, &now))
        || R_FAILED(timeToCalendarTimeWithMyRule(now, &calendar, &info))) return cached = "--:--";
    std::snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(calendar.hour), static_cast<unsigned>(calendar.minute));
#else
    const auto now = std::time(nullptr); std::tm calendar{};
    if (!localtime_r(&now, &calendar)) return cached = "--:--";
    std::snprintf(text, sizeof(text), "%02d:%02d", calendar.tm_hour, calendar.tm_min);
#endif
    return cached = text;
}
