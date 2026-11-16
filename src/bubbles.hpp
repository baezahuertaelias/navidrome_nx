#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct Bubble { float x, y, radius; };
constexpr size_t DefaultBubbleCount = 5;
constexpr size_t MaxBubbleCount = 10;

inline float reflectedPosition(float start, double velocity, double seconds, float lower, float upper) {
    const double range = upper - lower;
    if (range <= 0) return lower;
    double phase = std::fmod(std::clamp(start, lower, upper) - lower + velocity * seconds, range * 2);
    if (phase < 0) phase += range * 2;
    return static_cast<float>(lower + (phase <= range ? phase : range * 2 - phase));
}

inline Bubble bubbleAt(size_t index, double seconds, float width, float height, size_t count = DefaultBubbleCount) {
    constexpr std::array<Bubble, MaxBubbleCount> anchors{{
        {0.06f, 0.12f, 0.36f}, {0.87f, 0.23f, 0.30f}, {0.46f, 0.74f, 0.26f},
        {0.15f, 1.04f, 0.25f}, {0.98f, 0.95f, 0.38f},
        {0.34f, 0.20f, 0.25f}, {0.72f, 0.62f, 0.29f}, {0.10f, 0.55f, 0.27f},
        {0.66f, 0.03f, 0.24f}, {0.55f, 0.98f, 0.30f},
    }};
    const auto anchor = anchors.at(index);
    const float scale = 1.18f - 0.065f * (std::clamp(count, size_t{1}, MaxBubbleCount) - 1);
    if (width <= 0 || height <= 0) return {0, 0, 0};
    const float relativeRadius = anchor.radius * scale;
    const float minimum = std::min(width, height);
    const float rx = relativeRadius * (minimum / width), ry = relativeRadius * (minimum / height);
    const double vx = (0.012 + (index % 3) * 0.003) * (index % 2 ? -1 : 1);
    const double vy = (0.010 + (index % 4) * 0.002) * (index % 3 ? 1 : -1);
    return {width * reflectedPosition(anchor.x, vx, seconds, rx, 1 - rx),
        height * reflectedPosition(anchor.y, vy, seconds, ry, 1 - ry), minimum * relativeRadius};
}

inline constexpr double RainbowCycleSeconds = 180.0;
inline float rainbowHue(double seconds) {
    return static_cast<float>(std::fmod(seconds, RainbowCycleSeconds) / RainbowCycleSeconds);
}

inline float xmbWaveY(size_t layer, float position, double seconds, float height) {
    const double phase = layer * 0.9;
    return height * (0.52f + layer * 0.045f
        + 0.08f * static_cast<float>(std::sin(position * 6.283185307 + seconds * 0.18 + phase))
        + 0.035f * static_cast<float>(std::sin(position * 12.566370614 - seconds * 0.11 + phase)));
}
