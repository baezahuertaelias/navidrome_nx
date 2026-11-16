#include "audio_waveform.hpp"
#include <cassert>
#include <thread>
#include <vector>
int main() {
    AudioWaveform waveform;
    std::vector<int16_t> samples(AudioWaveform::Bins * 2);
    for (size_t i = 0; i < samples.size(); i += 2) { samples[i] = 32767; samples[i + 1] = -32768; }
    const auto original = samples;
    auto capture = [&] { waveform.capture(reinterpret_cast<const uint8_t*>(samples.data()), samples.size() * sizeof(int16_t)); };
    capture(); for (auto peak : waveform.snapshot()) assert(peak == 0);
    waveform.setState(WaveformState::Playing); capture();
    for (auto peak : waveform.snapshot()) assert(peak == 32768);
    assert(samples == original);
    assert(waveform.stereoMeanSquares()[0] == 32767u * 32767u);
    assert(waveform.stereoMeanSquares()[1] == 1073741824u);
    const auto beforePause = waveform.stereoMeanSquares();
    waveform.setState(WaveformState::Paused); std::fill(samples.begin(), samples.end(), 0); capture();
    for (auto peak : waveform.snapshot()) assert(peak == 32768);
    assert(waveform.stereoMeanSquares() == beforePause);
    waveform.setState(WaveformState::Playing); capture();
    for (auto peak : waveform.snapshot()) assert(peak == 0);
    assert(waveform.stereoMeanSquares()[0] == 0 && waveform.stereoMeanSquares()[1] == 0);
    for (size_t i = 0; i < samples.size(); i += 2) { samples[i] = 16384; samples[i + 1] = 0; }
    capture();
    assert(waveform.stereoMeanSquares()[0] == 268435456u && waveform.stereoMeanSquares()[1] == 0);
    assert(AudioWaveform::meterLevel(0) == 0 && AudioWaveform::meterLevel(1) == 0);
    assert(AudioWaveform::meterLevel(1073741824u) == 1);
    assert(AudioWaveform::meterLevel(268435456u) > 0.87f && AudioWaveform::meterLevel(268435456u) < 0.88f);
    waveform.channels = 1; samples.assign(AudioWaveform::Bins, 0); samples[23] = -12345;
    capture();
    assert(waveform.stereoMeanSquares()[0] == (12345u * 12345u) / AudioWaveform::Bins);
    assert(waveform.stereoMeanSquares()[0] == waveform.stereoMeanSquares()[1]);
    const auto impulse = waveform.snapshot();
    for (size_t i = 0; i < impulse.size(); ++i) assert(impulse[i] == (i == 23 ? 12345 : 0));
    waveform.capture(nullptr, 100); waveform.capture(reinterpret_cast<const uint8_t*>(samples.data()), 1);
    assert(waveform.snapshot() == impulse);
    std::thread producer([&] { for (int i = 0; i < 1000; ++i) capture(); });
    for (int i = 0; i < 1000; ++i) {
        for (auto peak : waveform.snapshot()) assert(peak <= 32768);
        for (auto level : waveform.stereoMeanSquares()) assert(level <= 1073741824u);
    }
    producer.join();
    waveform.setState(WaveformState::Stopped);
    for (auto peak : waveform.snapshot()) assert(peak == 0);
    for (auto level : waveform.stereoMeanSquares()) assert(level == 0);
}
