#pragma once
#include <string>
#include <vector>

struct LyricLine { std::string text; long long startMs = -1; };
struct SongLyrics {
    std::vector<LyricLine> lines;
    bool synced = false;
    long long offsetMs = 0;
    bool empty() const { return lines.empty(); }
    int activeLine(long long elapsedMs) const {
        if (!synced) return -1;
        int active = -1;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (static_cast<long double>(lines[i].startMs) + offsetMs > elapsedMs) break;
            active = static_cast<int>(i);
        }
        return active;
    }
};
