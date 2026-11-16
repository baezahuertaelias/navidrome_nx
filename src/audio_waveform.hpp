#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

enum class WaveformState { Stopped, Playing, Paused };
class AudioWaveform {
public:
    static constexpr size_t Bins = 48;
    static_assert(std::atomic<uint32_t>::is_always_lock_free, "Audio capture must not block");
    int channels = 2; // Set before attaching the mixer callback.
private:
    std::array<std::atomic<uint32_t>, Bins> peaks{};
    std::atomic<WaveformState> state{WaveformState::Stopped};
public:
    WaveformState getState() const { return state.load(std::memory_order_relaxed); }
    void setState(WaveformState value) {
        state.store(value, std::memory_order_relaxed);
        if (value == WaveformState::Stopped)
            for (auto& peak : peaks) peak.store(0, std::memory_order_relaxed);
    }
    std::array<uint32_t, Bins> snapshot() const {
        std::array<uint32_t, Bins> result;
        for (size_t i = 0; i < Bins; ++i) result[i] = peaks[i].load(std::memory_order_relaxed);
        return result;
    }
    void capture(const uint8_t* bytes, size_t length) {
        if (getState() != WaveformState::Playing || !bytes || channels < 1 || channels > 8) return;
        const size_t frames = length / (sizeof(int16_t) * channels);
        if (!frames) return;
        for (size_t bin = 0; bin < Bins; ++bin) {
            const size_t first = frames * bin / Bins;
            const size_t end = std::min(frames, std::max(first + 1, frames * (bin + 1) / Bins));
            uint32_t peak = 0;
            for (size_t frame = first; frame < end; ++frame) {
                for (int channel = 0; channel < channels; ++channel) {
                    int16_t sample;
                    std::memcpy(&sample, bytes + (frame * channels + channel) * sizeof(sample), sizeof(sample));
                    const int value = sample;
                    peak = std::max(peak, static_cast<uint32_t>(value < 0 ? -value : value));
                }
            }
            peaks[bin].store(peak, std::memory_order_relaxed);
        }
    }
    static void postMix(void* context, uint8_t* stream, int length) {
        if (length > 0) static_cast<AudioWaveform*>(context)->capture(stream, static_cast<size_t>(length));
    }
};
