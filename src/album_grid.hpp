#pragma once
#include <cstddef>
#include <algorithm>

constexpr int AlbumColumns = 4;
constexpr float AlbumGap = 12;
constexpr float AlbumListPadding = 20;

inline float albumCardWidth(float listWidth, int columns = AlbumColumns) {
    return (listWidth - AlbumListPadding - (columns - 1) * AlbumGap) / columns;
}

inline int albumVerticalNeighbor(size_t index, size_t count, int direction, int columns = AlbumColumns) {
    const int next = static_cast<int>(index) + direction * columns;
    return next >= 0 && static_cast<size_t>(next) < count ? next : -1;
}

inline float albumArtSize(float width, float height, int columns) {
    return std::min(albumCardWidth(width, columns) - 20, std::max(60.0f, height - 100));
}
