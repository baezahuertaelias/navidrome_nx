#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

inline constexpr size_t WarpStarCount = 80;
struct WarpStar { float x = 0, y = 0, tailX = 0, tailY = 0, brightness = 0, width = 0; };
inline WarpStar warpStarAt(size_t index, double seconds, float width, float height) {
    if (width <= 0 || height <= 0) return {};
    uint32_t seed = static_cast<uint32_t>(index + 1) * 747796405u + 2891336453u;
    seed = (seed ^ (seed >> 16)) * 2246822519u;
    seed ^= seed >> 13;
    const double angle = seed * (6.283185307179586 / 4294967296.0);
    const float dx = static_cast<float>(std::cos(angle)), dy = static_cast<float>(std::sin(angle));
    double phase = std::fmod(index * 0.618033988749895 + seconds * (0.09 + (seed >> 24) * (0.04 / 255)), 1.0);
    if (phase < 0) phase += 1;
    const float travel = static_cast<float>(std::expm1(phase * 3) / std::expm1(3.0));
    const float tail = std::max(0.0f, travel - 0.012f - 0.045f * travel);
    const float distance = std::min(width * 0.5f / std::max(0.0001f, std::abs(dx)), height * 0.5f / std::max(0.0001f, std::abs(dy)));
    const float fade = std::clamp(static_cast<float>(std::min(phase, 1 - phase) * 8), 0.0f, 1.0f);
    const float scale = std::min(width / 1280, height / 720);
    return {width / 2 + dx * distance * travel, height / 2 + dy * distance * travel,
        width / 2 + dx * distance * tail, height / 2 + dy * distance * tail,
        fade * (0.2f + 0.8f * travel), scale * (0.6f + 1.0f * travel)};
}
