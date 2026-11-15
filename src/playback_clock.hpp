#pragma once
#include <algorithm>
#include <chrono>
#include <string>

inline std::string playbackTime(long long seconds) {
    seconds = std::max(0LL, seconds);
    return std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
}

class PlaybackClock {
    using Clock = std::chrono::steady_clock;
    Clock::time_point started;
    Clock::duration accumulated{};
    bool running = false;
public:
    void reset() { accumulated = Clock::duration::zero(); running = false; }
    void resume(Clock::time_point now = Clock::now()) {
        if (!running) { started = now; running = true; }
    }
    void pause(Clock::time_point now = Clock::now()) {
        if (running) { accumulated += now - started; running = false; }
    }
    long long seconds(Clock::time_point now = Clock::now()) const {
        return std::chrono::duration_cast<std::chrono::seconds>(accumulated + (running ? now - started : Clock::duration::zero())).count();
    }
};
