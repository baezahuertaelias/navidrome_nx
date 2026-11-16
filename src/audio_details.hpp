#pragma once
#include <cctype>
#include "api.hpp"

inline constexpr int StreamBitRate = 192;
inline std::string audioDetails(const Entry& track) {
    std::string text;
    auto append = [&text](const std::string& value) { if (!text.empty()) text += " · "; text += value; };
    if (!track.suffix.empty() && track.suffix.size() <= 16
        && std::all_of(track.suffix.begin(), track.suffix.end(), [](unsigned char c) { return std::isalnum(c); })) {
        auto format = track.suffix;
        for (auto& c : format) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        append(format);
    }
    if (track.bitRate > 0) append(std::to_string(track.bitRate) + " kbps");
    if (track.bitDepth > 0) append(std::to_string(track.bitDepth) + "-bit");
    if (track.samplingRate > 0) {
        auto rate = std::to_string(track.samplingRate / 1000);
        if (track.samplingRate % 1000) {
            auto fraction = std::to_string(1000 + track.samplingRate % 1000).substr(1);
            while (fraction.back() == '0') fraction.pop_back();
            rate += "." + fraction;
        }
        append(rate + " kHz");
    }
    if (track.channelCount > 0) append(std::to_string(track.channelCount) + " ch");
    return text;
}
