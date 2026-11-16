#pragma once
#include "music_ui.hpp"
#include <cassert>
#include <chrono>
#include "bubbles.hpp"
#include "settings.hpp"
#include "warp.hpp"

// Native gradient circles keep the animation independent of image assets and frame rate.
class MusicSurface : public aurore::Box {
    std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
    double animationTime = 0;
public:
    MusicSurface() : aurore::Box(aurore::Axis::COLUMN) {}
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        aurore::Style style, aurore::FrameContext* context) override {
        const auto now = std::chrono::steady_clock::now();
        const double colorSeconds = std::chrono::duration<double>(now.time_since_epoch()).count();
        const float hue = rainbowHue(colorSeconds);
        auto shade = [](NVGcolor color) {
            const float opacity = preferences.backgroundOpacity / 100.0f;
            color.r *= opacity; color.g *= opacity; color.b *= opacity;
            return color;
        };
        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        const std::array<NVGcolor, 4> topColors{{nvgRGB(48, 23, 77), nvgRGB(18, 43, 86), nvgRGB(76, 24, 55), nvgRGB(16, 61, 58)}};
        const std::array<NVGcolor, 4> bottomColors{{nvgRGB(16, 14, 38), nvgRGB(10, 18, 40), nvgRGB(30, 12, 34), nvgRGB(8, 25, 37)}};
        const bool colored = preferences.background != 4 && preferences.background != 6;
        if (!colored) {
            nvgFillColor(vg, nvgRGB(0, 0, 0));
            nvgFill(vg);
        } else if (preferences.background == 5) {
            nvgFillColor(vg, shade(nvgHSL(hue, 0.3f, 0.12f)));
            nvgFill(vg);
        } else {
            const auto glow = backgroundGlow(colorSeconds, width, height);
            nvgFillPaint(vg, nvgRadialGradient(vg, x + glow.x, y + glow.y, 0, glow.radius,
                shade(nvgLerpRGBA(bottomColors[preferences.background], topColors[preferences.background], 0.7f)), shade(bottomColors[preferences.background])));
            nvgFill(vg);
        }
        animationTime += std::chrono::duration<double>(now - lastFrame).count() * bubbleRate(preferences.bubbleSpeed);
        lastFrame = now;
        const double seconds = animationTime;
        const std::array<NVGcolor, DefaultBubbleCount> colors{{
            nvgRGBA(161, 91, 240, 80), nvgRGBA(185, 111, 247, 65), nvgRGBA(144, 94, 231, 35),
            nvgRGBA(127, 98, 235, 60), nvgRGBA(182, 89, 228, 65),
        }};
        nvgSave(vg);
        nvgIntersectScissor(vg, x, y, width, height);
        for (size_t i = 0; colored && preferences.backgroundStyle == 0 && i < static_cast<size_t>(preferences.bubbleCount); ++i) {
            const auto bubble = bubbleAt(i, seconds, width, height, preferences.bubbleCount);
            const float cx = x + bubble.x, cy = y + bubble.y;
            const auto color = preferences.background == 5 ? nvgHSLA(rainbowHue(colorSeconds + i * 4), 0.45f, 0.6f, 65) : colors[i % colors.size()];
            auto transparent = color;
            transparent.a = 0;
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, bubble.radius);
            nvgFillPaint(vg, nvgRadialGradient(vg, cx - bubble.radius * 0.2f, cy - bubble.radius * 0.25f,
                bubble.radius * 0.08f, bubble.radius * 1.25f, color, transparent));
            nvgFill(vg);
            nvgStrokeWidth(vg, 1);
            nvgStrokeColor(vg, nvgRGBA(218, 184, 255, 5));
            nvgStroke(vg);
        }
        if (colored && preferences.backgroundStyle == 1) {
            constexpr int segments = 64;
            for (size_t layer = 0; layer < 3; ++layer) {
                const auto color = preferences.background == 5 ? nvgHSLA(rainbowHue(colorSeconds + layer * 4), 0.35f, 0.75f, 16)
                    : nvgRGBA(207, 193, 255, 16);
                auto transparent = color; transparent.a = 0;
                nvgBeginPath(vg);
                for (int sample = 0; sample <= segments; ++sample) {
                    const float position = static_cast<float>(sample) / segments;
                    const float py = y + xmbWaveY(layer, position, seconds, height);
                    if (sample == 0) nvgMoveTo(vg, x, py);
                    else nvgLineTo(vg, x + width * position, py);
                }
                for (int sample = segments; sample >= 0; --sample) {
                    const float position = static_cast<float>(sample) / segments;
                    const float thickness = height * (0.025f + 0.02f * std::sin(position * 3.141592654f));
                    nvgLineTo(vg, x + width * position, y + xmbWaveY(layer, position, seconds, height) + thickness);
                }
                nvgClosePath(vg);
                nvgFillPaint(vg, nvgLinearGradient(vg, x, y + height * 0.38f, x, y + height * 0.78f, color, transparent));
                nvgFill(vg);
            }
        }
        if (preferences.background == 6) {
            for (size_t i = 0; i < WarpStarCount; ++i) {
                const auto star = warpStarAt(i, seconds, width, height);
                if (star.brightness <= 0) continue;
                const auto bright = nvgRGBA(187, 214, 255, static_cast<unsigned char>(star.brightness * 150));
                const auto clear = nvgRGBA(187, 214, 255, 0);
                nvgBeginPath(vg); nvgMoveTo(vg, x + star.tailX, y + star.tailY); nvgLineTo(vg, x + star.x, y + star.y);
                nvgStrokeWidth(vg, star.width);
                nvgStrokePaint(vg, nvgLinearGradient(vg, x + star.tailX, y + star.tailY, x + star.x, y + star.y, clear, bright));
                nvgStroke(vg);
                nvgBeginPath(vg); nvgCircle(vg, x + star.x, y + star.y, star.width * 0.65f);
                nvgFillColor(vg, bright); nvgFill(vg);
            }
        }
        nvgRestore(vg);
        aurore::Box::draw(vg, x, y, width, height, style, context);
    }
};

inline void applyMusicTheme() {
    for (auto* theme : {&aurore::Theme::getLightTheme(), &aurore::Theme::getDarkTheme()}) {
        theme->addColor("aurore/background", nvgRGB(18, 25, 45));
        theme->addColor("aurore/text", nvgRGB(245, 246, 255));
        theme->addColor("aurore/highlight/background", nvgRGB(47, 50, 83));
        theme->addColor("aurore/highlight/color1", nvgRGB(104, 218, 255));
        theme->addColor("aurore/highlight/color2", nvgRGB(160, 139, 255));
        theme->addColor("aurore/button/default_enabled_background", nvgRGBA(98, 108, 152, 44));
        theme->addColor("aurore/button/default_disabled_background", nvgRGBA(98, 108, 152, 20));
        theme->addColor("aurore/button/default_enabled_text", nvgRGB(244, 245, 255));
        theme->addColor("aurore/button/default_disabled_text", nvgRGB(135, 143, 168));
        theme->addColor("aurore/button/primary_enabled_background", nvgRGB(119, 76, 220));
        theme->addColor("aurore/button/primary_enabled_text", nvgRGB(255, 255, 255));
        theme->addColor("aurore/button/primary_disabled_background", nvgRGB(49, 45, 75));
        theme->addColor("aurore/button/primary_disabled_text", nvgRGB(145, 141, 164));
        assert((*theme)["aurore/button/primary_enabled_background"].r == nvgRGB(119, 76, 220).r);
        assert((*theme)["aurore/button/default_disabled_text"].r == nvgRGB(135, 143, 168).r);
    }
}
