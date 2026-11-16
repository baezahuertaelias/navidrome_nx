#pragma once
#include <algorithm>
#include <cctype>
#include <random>
#include "api.hpp"

enum class LibraryMode { Home, Albums, Artists, Songs, Favorites, ArtistAlbums, AlbumTracks, Playlists, PlaylistTracks };
enum class LibraryView { List, Grid, CoverFlow };
inline size_t libraryViewSlot(LibraryMode mode, bool favoriteSongs) {
    return mode == LibraryMode::Favorites && !favoriteSongs ? 9 : static_cast<size_t>(mode);
}
inline bool isCollection(LibraryMode mode) {
    return mode == LibraryMode::Albums || mode == LibraryMode::Artists || mode == LibraryMode::Songs;
}
inline int compareNames(const std::string& a, const std::string& b) {
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        const auto left = std::tolower(static_cast<unsigned char>(a[i]));
        const auto right = std::tolower(static_cast<unsigned char>(b[i]));
        if (left != right) return left < right ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : a.size() < b.size() ? -1 : 1;
}
inline void sortEntries(std::vector<Entry>& entries, EntryKind kind, int order) {
    std::stable_sort(entries.begin(), entries.end(), [kind, order](const Entry& a, const Entry& b) {
        if (kind == EntryKind::Artist) {
            const auto title = compareNames(a.title, b.title);
            if (title) return order == 1 ? title > 0 : title < 0;
        } else {
            const auto artist = compareNames(a.artist, b.artist);
            if (order == 1 && artist) return artist < 0;
            if (order == 2 && a.created != b.created) return a.created > b.created;
            const auto title = compareNames(a.title, b.title);
            if (title) return title < 0;
        }
        return a.id < b.id;
    });
}
inline void sortAlbums(std::vector<Entry>& entries, int order) { sortEntries(entries, EntryKind::Album, order); }
template<class Generator> std::vector<Entry> shuffledFavorites(std::vector<Entry> songs, Generator& generator) {
    std::shuffle(songs.begin(), songs.end(), generator);
    return songs;
}
