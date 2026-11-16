#include "warp.hpp"
#include <cassert>
#include <iostream>
int main() {
    assert(warpStarAt(0, 0, 0, 720).brightness == 0);
    assert(warpStarAt(0, 0, 1280, 0).width == 0);
    bool moved = false, visible = false;
    for (size_t i = 0; i < WarpStarCount; ++i) {
        for (double seconds : {-10.0, 0.0, 0.5, 10.0, 300.0, 86400.0}) {
            const auto star = warpStarAt(i, seconds, 1280, 720);
            const auto scaled = warpStarAt(i, seconds, 1920, 1080);
            const auto next = warpStarAt(i, seconds + 1.0 / 60, 1280, 720);
            assert(std::isfinite(star.x) && std::isfinite(star.y));
            assert(star.x >= 0 && star.x <= 1280 && star.y >= 0 && star.y <= 720);
            assert(star.tailX >= 0 && star.tailX <= 1280 && star.tailY >= 0 && star.tailY <= 720);
            assert(star.brightness >= 0 && star.brightness <= 1 && star.width > 0 && star.width <= 1.6f);
            assert(std::hypot(star.x - 640, star.y - 360) >= std::hypot(star.tailX - 640, star.tailY - 360));
            assert(std::abs(scaled.x - star.x * 1.5f) < 0.001f);
            assert(std::abs(scaled.y - star.y * 1.5f) < 0.001f);
            assert(std::abs(scaled.width - star.width * 1.5f) < 0.001f);
            if (std::hypot(next.x - star.x, next.y - star.y) > 100) assert(star.brightness < 0.03f && next.brightness < 0.03f);
            else assert(std::hypot(next.x - star.x, next.y - star.y) < 12);
            visible |= star.brightness > 0.3f;
            moved |= std::hypot(next.x - star.x, next.y - star.y) > 0.1f;
        }
    }
    assert(moved && visible);
    std::cout << "Warp motion, fade-on-wrap, bounds and resolution scaling checks passed\n";
}
