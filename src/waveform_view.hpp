#pragma once
#include <chrono>
#include <cmath>
#include "audio_waveform.hpp"

class WaveformView : public brls::View {
    const AudioWaveform& audio;
    std::array<float, AudioWaveform::Bins> displayed{};
    std::chrono::steady_clock::time_point lastDraw = std::chrono::steady_clock::now();
public:
    explicit WaveformView(const AudioWaveform& audio) : audio(audio) { setDimensions(140, 30); setShrink(0); }
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style, brls::FrameContext*) override {
        const auto now = std::chrono::steady_clock::now();
        const float seconds = std::min(0.1f, std::chrono::duration<float>(now - lastDraw).count());
        lastDraw = now;
        const auto state = audio.getState();
        if (state == WaveformState::Stopped) displayed.fill(0);
        else if (state == WaveformState::Playing) {
            const auto peaks = audio.snapshot();
            const float blend = 1 - std::exp(-seconds * 20);
            for (size_t i = 0; i < displayed.size(); ++i) {
                const float target = std::sqrt(peaks[i] / 32768.0f);
                displayed[i] += (target - displayed[i]) * blend;
            }
        }
        const float middle = y + height / 2, amplitude = height / 2 - 2;
        nvgSave(vg); nvgIntersectScissor(vg, x, y, width, height);
        nvgBeginPath(vg);
        for (size_t i = 0; i < displayed.size(); ++i) {
            const float px = x + width * i / (displayed.size() - 1), py = middle - displayed[i] * amplitude;
            if (!i) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
        }
        for (size_t i = displayed.size(); i-- > 0;)
            nvgLineTo(vg, x + width * i / (displayed.size() - 1), middle + displayed[i] * amplitude);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(116, 220, 255, 28)); nvgFill(vg);
        nvgStrokeWidth(vg, 1.4f); nvgStrokeColor(vg, nvgRGBA(116, 220, 255, state == WaveformState::Playing ? 210 : 95)); nvgStroke(vg);
        nvgRestore(vg);
    }
};
