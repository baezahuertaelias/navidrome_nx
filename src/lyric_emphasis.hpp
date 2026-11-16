#pragma once
#include <algorithm>

class LyricEmphasis {
    int active = -1, previous = -1;
    double changed = 0;
    float previousWeight = 0;
public:
    float amount(int line, double seconds) const {
        const float progress = static_cast<float>(std::clamp((seconds - changed) / 0.25, 0.0, 1.0));
        const float eased = progress * progress * (3 - 2 * progress);
        if (line >= 0 && line == active) return eased;
        if (line >= 0 && line == previous) return previousWeight * (1 - eased);
        return 0;
    }
    void update(int line, double seconds) {
        if (line == active) return;
        previousWeight = amount(active, seconds);
        previous = active; active = line; changed = seconds;
    }
};
