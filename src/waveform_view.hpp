#pragma once
#include <chrono>
#include <cmath>
#include "audio_waveform.hpp"
#include "settings.hpp"

class WaveformView : public aurore::View {
    const AudioWaveform& audio;
    std::array<float, AudioWaveform::Bins> displayed{};
    std::array<float, 2> levels{};
    std::chrono::steady_clock::time_point lastDraw = std::chrono::steady_clock::now();
public:
    explicit WaveformView(const AudioWaveform& audio) : audio(audio) { setDimensions(140, 30); setShrink(0); }
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        aurore::Style, aurore::FrameContext*) override {
        const auto now = std::chrono::steady_clock::now();
        const float seconds = std::min(0.1f, std::chrono::duration<float>(now - lastDraw).count());
        lastDraw = now;
        const auto state = audio.getState();
        if (state == WaveformState::Stopped) { displayed.fill(0); levels.fill(0); }
        else if (state == WaveformState::Playing) {
            const auto peaks = audio.snapshot();
            const float blend = 1 - std::exp(-seconds * 20);
            for (size_t i = 0; i < displayed.size(); ++i) {
                const float target = std::sqrt(peaks[i] / 32768.0f);
                displayed[i] += (target - displayed[i]) * blend;
            }
            const auto stereo = audio.stereoMeanSquares();
            for (size_t channel = 0; channel < levels.size(); ++channel) {
                const float target = AudioWaveform::meterLevel(stereo[channel]);
                const float speed = target > levels[channel] ? 35 : 6;
                levels[channel] += (target - levels[channel]) * (1 - std::exp(-seconds * speed));
            }
        }
        const float middle = y + height / 2, amplitude = height / 2 - 2;
        nvgSave(vg); nvgIntersectScissor(vg, x, y, width, height);
        if (preferences.visualizer == 1) {
            constexpr size_t bars = 16, samples = AudioWaveform::Bins / bars;
            for (size_t bar = 0; bar < bars; ++bar) {
                float peak = 0;
                for (size_t sample = 0; sample < samples; ++sample) peak = std::max(peak, displayed[bar * samples + sample]);
                const float barHeight = std::max(1.0f, peak * (height - 2));
                const float left = x + width * bar / bars, barWidth = std::max(1.0f, width / bars - 2);
                nvgBeginPath(vg); nvgRoundedRect(vg, left, y + height - barHeight, barWidth, barHeight, 1);
                nvgFillPaint(vg, nvgLinearGradient(vg, left, y, left, y + height,
                    nvgRGBA(169, 120, 250, 220), nvgRGBA(116, 220, 255, state == WaveformState::Playing ? 210 : 95)));
                nvgFill(vg);
            }
            nvgRestore(vg); return;
        }
        if (preferences.visualizer == 2) {
            constexpr int segments = 18;
            const float labelWidth = 14, step = (width - labelWidth) / segments;
            nvgFontFaceId(vg, aurore::Application::getFont(aurore::FONT_REGULAR));
            nvgFontSize(vg, 10); nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            for (size_t channel = 0; channel < levels.size(); ++channel) {
                const float top = y + 2 + channel * (height / 2);
                nvgFillColor(vg, nvgRGBA(190, 198, 220, 210));
                nvgText(vg, x, top + 4, channel ? "R" : "L", nullptr);
                for (int segment = 0; segment < segments; ++segment) {
                    auto color = segment >= 16 ? nvgRGB(251, 104, 122) : segment >= 13 ? nvgRGB(249, 208, 94) : nvgRGB(116, 229, 172);
                    color.a = levels[channel] * segments > segment ? 0.9f : 0.12f;
                    nvgBeginPath(vg); nvgRoundedRect(vg, x + labelWidth + segment * step, top, std::max(1.0f, step - 1), 8, 1);
                    nvgFillColor(vg, color); nvgFill(vg);
                }
            }
            nvgRestore(vg); return;
        }
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
