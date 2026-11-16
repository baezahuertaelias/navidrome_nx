#pragma once
#include <array>
#include <algorithm>
#include <functional>
#include "api.hpp"

constexpr size_t HomeRowLimit = 12;
struct HomeSection {
    const char* title;
    std::vector<Entry> entries;
    bool unavailable = false;
};
using HomeSections = std::array<HomeSection, 5>;
inline HomeSections fetchHome(Api& api, const std::function<std::string()>& salt) {
    HomeSections rows{{{"app/most_played", {}}, {"app/home_favorites", {}},
        {"app/play_again", {}}, {"app/discover", {}}, {"app/random_songs", {}}}};
    const char* albumTypes[] = {"frequent", "", "recent", "random", ""};
    for (size_t i = 0; i < rows.size(); ++i) {
        try {
            if (i == 1) {
                auto body = Api::response(api.request("getStarred2", {}, salt()));
                rows[i].entries = Api::entries(body.get(), "starred2", "song");
            } else if (i == 4) {
                auto body = Api::response(api.request("getRandomSongs", {{"size", std::to_string(HomeRowLimit)}}, salt()));
                rows[i].entries = Api::entries(body.get(), "randomSongs", "song");
            } else {
                auto body = Api::response(api.request("getAlbumList2", {{"type", albumTypes[i]}, {"size", std::to_string(HomeRowLimit)}}, salt()));
                rows[i].entries = Api::entries(body.get(), "albumList2", "album");
            }
            if (rows[i].entries.size() > HomeRowLimit) rows[i].entries.resize(HomeRowLimit);
        } catch (const std::exception&) { rows[i].unavailable = true; }
    }
    return rows;
}
constexpr float HomeCardWidth = 180, HomeCardGap = 12, HomeRailPadding = 8;
inline float homeRailOffset(size_t index, size_t count, float width, float offset) {
    if (!count || index >= count || width <= 0) return 0;
    const float left = index * (HomeCardWidth + HomeCardGap);
    const float right = left + HomeCardWidth + HomeRailPadding * 2;
    if (left < offset) offset = left;
    if (right > offset + width) offset = right - width;
    const float total = count * HomeCardWidth + (count - 1) * HomeCardGap + HomeRailPadding * 2;
    return std::clamp(offset, 0.0f, std::max(0.0f, total - width));
}
