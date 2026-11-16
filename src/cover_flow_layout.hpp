#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

inline constexpr int CoverFlowRadius = 3;
inline float coverFlowArtSize(float width, float height) {
    return std::max(48.0f, std::min({260.0f, width * 0.26f, height - 164}));
}
struct CoverFlowPose { float center = 0, scaleX = 1, scaleY = 1, shear = 0, alpha = 1; };
inline CoverFlowPose coverFlowPose(float distance, float art) {
    const float absolute = std::abs(distance), tilt = std::min(1.0f, absolute);
    const float spread = absolute <= 1 ? absolute * 0.83f : 0.83f + (absolute - 1) * 0.38f;
    return {(distance < 0 ? -1 : 1) * art * spread, 1 - 0.5f * tilt,
        1 - 0.12f * tilt, (distance < 0 ? -1 : 1) * 0.13f * tilt, 1 / (1 + 0.18f * absolute)};
}
inline bool coverFlowHit(float x, float y, float centerY, float art, const CoverFlowPose& pose) {
    const float localX = (x - pose.center) / pose.scaleX;
    const float localY = (y - centerY - pose.shear * localX) / pose.scaleY;
    return std::abs(localX) <= art / 2 && std::abs(localY) <= art / 2;
}
inline size_t coverFlowMove(size_t index, size_t count, int direction) {
    if (!count) return 0;
    return static_cast<size_t>(std::clamp(static_cast<long long>(std::min(index, count - 1)) + direction, 0LL, static_cast<long long>(count) - 1));
}

inline std::array<size_t, CoverFlowRadius * 2 + 1> coverFlowDrawOrder(size_t selected, size_t count, float position) {
    std::array<size_t, CoverFlowRadius * 2 + 1> result{}; result.fill(count);
    for (int offset = -CoverFlowRadius; offset <= CoverFlowRadius; ++offset) {
        const auto index = static_cast<long long>(selected) + offset;
        if (index >= 0 && static_cast<size_t>(index) < count) result[offset + CoverFlowRadius] = index;
    }
    auto distance = [count, position](size_t index) { return index < count ? std::abs(static_cast<float>(index) - position) : -1.0f; };
    for (size_t i = 0; i < result.size(); ++i)
        for (size_t j = i + 1; j < result.size(); ++j)
            if (distance(result[j]) > distance(result[i])) std::swap(result[i], result[j]);
    return result;
}
