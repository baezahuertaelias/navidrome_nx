#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

struct LyricsNavigation {
    int selected = 0, active = -1;
    bool manual = false, follow = false;
    float touchOffset = 0;
    void reset(bool synced) { selected = 0; active = -1; manual = false; follow = synced; touchOffset = 0; }
    void setManual(bool enabled, bool synced) {
        manual = enabled; follow = !enabled && synced; touchOffset = 0;
        if (!enabled) selected = std::max(0, active);
    }
    void move(int direction, size_t count) {
        if (!manual || !count) return;
        follow = false;
        selected = std::clamp(selected + direction, 0, static_cast<int>(count) - 1);
    }
    bool scroll(float dy, size_t count) {
        if (!manual || !dy || !count) return false;
        touchOffset += dy;
        while (std::abs(touchOffset) >= 36) {
            const int direction = touchOffset > 0 ? -1 : 1;
            move(direction, count); touchOffset += direction * 36;
        }
        return true;
    }
    void updateActive(int line) { active = line; if (follow && active >= 0) selected = active; }
};
