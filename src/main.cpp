#include <switch.h>
#include <borealis.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <chrono>
#include <future>
#include <functional>
#include <map>
#include "api.hpp"
#include "playback_clock.hpp"
#include "music_surface.hpp"
#include "album_grid.hpp"
#include "lyrics_screen.hpp"
#include "settings_screen.hpp"
#include "library_logic.hpp"
#include "remote_server.hpp"
#include "home_rail.hpp"
#include "login_screen.hpp"
#include "waveform_view.hpp"
#include "audio_details.hpp"
#include "catalog.hpp"
#include "cover_flow.hpp"
#include "song_cache.hpp"

static std::string localizedError(const std::exception& error) {
    if (const auto* api = dynamic_cast<const ApiError*>(&error))
        return brls::getStr("app/error/" + api->key, api->detail);
    return error.what(); // Preserve technical details from SDL and the runtime.
}

static std::string artist(const Entry& entry) {
    return entry.artist.empty() ? brls::getStr("app/unknown_artist") : entry.artist;
}

static std::string salt() {
    unsigned char random[16];
    randomGet(random, sizeof(random));
    std::string text;
    for (auto byte : random) {
        text += "0123456789abcdef"[byte >> 4];
        text += "0123456789abcdef"[byte & 15];
    }
    return text;
}

class App : public brls::Activity {
    std::unique_ptr<Api> api;
    brls::Label* status = nullptr;
    brls::ScrollingFrame* scroll = nullptr;
    brls::Button* reload = nullptr;
    brls::Button *back = nullptr, *pagePrevious = nullptr, *pageNext = nullptr;
    brls::Button *previous = nullptr, *pause = nullptr, *next = nullptr, *stopButton = nullptr;
    brls::Label *heading = nullptr, *libraryInfo = nullptr, *playerState = nullptr;
    brls::Label *trackTitle = nullptr, *trackArtist = nullptr, *queueInfo = nullptr, *time = nullptr;
    brls::Rectangle* progress = nullptr;
    brls::Label *sourceInfo = nullptr, *pageIndicator = nullptr;
    brls::Box* pagination = nullptr;
    brls::Button* viewToggle = nullptr;
    Catalog artistCatalog, songCatalog;
    std::array<LibraryView, 10> libraryViews{{LibraryView::List, LibraryView::Grid, LibraryView::List, LibraryView::List,
        LibraryView::List, LibraryView::Grid, LibraryView::List, LibraryView::List, LibraryView::List, LibraryView::Grid}};
    std::array<std::string, 10> flowSelections{};
    int artistParentOffset = 0;
    std::string selectedArtistId;
    Entry openedAlbum, openedPlaylist;
    std::vector<Entry> playlists;
    bool playlistsLoaded = false;
    int playlistParentOffset = 0;
    brls::Button* playlistsNav = nullptr;
    brls::Box* playerPanel = nullptr;
    brls::Button *favorites = nullptr, *favoriteKind = nullptr, *lyricsButton = nullptr;
    brls::Button *albumsNav = nullptr, *lyricsNav = nullptr;
    brls::Image* playerCover = nullptr;
    std::vector<std::pair<std::string, brls::Image*>> coverTargets;
    std::map<std::string, std::vector<char>> coverCache;
    std::future<std::pair<std::string, std::vector<char>>> coverJob;
    std::string playerCoverId;
    LyricsScreen* lyricsScreen = nullptr;
    bool reloadLyrics = false, logoutRequested = false;
    SettingsScreen* settingsScreen = nullptr;
    LoginScreen* loginScreen = nullptr;
    bool loginRequested = false, loginSavedRequested = false;
    RemoteServer remote;
    std::string remoteError, remoteNotice;
    std::vector<Entry> remoteResults;
    bool remoteResultsSet = false, suppressPlayback = false;
    std::string remoteView;
    std::chrono::steady_clock::time_point remotePublished{};
    brls::Box *sidebar = nullptr, *library = nullptr;
    brls::Label *clockLabel = nullptr, *brandTitle = nullptr, *tagline = nullptr;
    brls::Button *collapseButton = nullptr, *artistsNav = nullptr, *songsNav = nullptr, *settingsNav = nullptr;
    brls::Button *searchButton = nullptr, *orderButton = nullptr, *shuffleButton = nullptr, *clearSearch = nullptr;
    std::vector<std::pair<brls::Button*, std::pair<std::string, std::string>>> menuLabels;
    std::vector<Entry> displayEntries, searchAlbums, artistAlbums;
    brls::Button* homeNav = nullptr;
    HomeSections homeSections{};
    bool homeLoaded = false;
    size_t homeSelectedRow = 0, homeSelectedItem = 0;
    LibraryMode mode = LibraryMode::Home;
    LibraryMode returnMode = LibraryMode::Albums;
    int returnOffset = 0, albumOrder = 0;
    std::string albumQuery, selectedArtist;
    bool searchCacheValid = false, needsRerender = false;
    brls::View* preserveFocus = nullptr;
    std::string lastClock;

    std::vector<Entry> favoriteAlbums, favoriteTracks;
    bool favoritesMode = false, favoriteSongs = false, lyricsView = false;
    std::vector<Entry> cachedAlbums;
    std::vector<std::pair<std::string, brls::Box*>> trackRows;
    std::string selectedAlbum;
    PlaybackClock clock;
    AudioWaveform waveform;
    long long lastSecond = -1;
    bool viewingAlbums = true, restoreAlbums = false, checkLayout = false;
    NVGcolor accent, muted, panel;
    std::future<std::function<void()>> job;
    std::vector<Entry> queue;
    std::vector<char> audio;
    Mix_Music* music = nullptr;
    size_t current = 0;
    int offset = 0;
    bool audioReady = false;

    brls::Image* cover(brls::Box* parent, float size) {
        auto image = new brls::Image();
        image->setDimensions(size, size);
        image->setShrink(0);
        image->setMarginRight(12);
        image->setBackgroundColor(panel);
        parent->addView(image);
        return image;
    }
    void pollCovers() {
        if (coverJob.valid() && coverJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = coverJob.get();
            if (coverCache.size() >= 64) {
                const auto unused = std::find_if(coverCache.begin(), coverCache.end(), [this](const auto& cached) {
                    if (cached.first == "768:" + playerCoverId) return false;
                    return std::none_of(coverTargets.begin(), coverTargets.end(), [&](const auto& target) { return cached.first == "256:" + target.first; });
                });
                assert(unused != coverCache.end()); // Home has at most 60 covers plus the player.
                if (unused != coverCache.end()) coverCache.erase(unused);
            }
            coverCache[result.first] = std::move(result.second);
        }
        auto targets = coverTargets;
        targets.insert(targets.begin(), {playerCoverId, playerCover});
        for (const auto& target : targets) {
            if (target.first.empty() || target.second->getTexture()) continue;
            if (lyricsScreen && target.second != playerCover) continue;
            if (target.second != playerCover && (target.second->getY() + target.second->getHeight() < scroll->getY()
                || target.second->getY() > scroll->getY() + scroll->getHeight())) continue;
            const bool hero = target.second == playerCover;
            const auto key = std::string(hero ? "768:" : "256:") + target.first;
            auto cached = coverCache.find(key);
            if (cached != coverCache.end()) {
                if (!cached->second.empty()) {
                    if (!target.second->setImageFromMemory(reinterpret_cast<const unsigned char*>(cached->second.data()), static_cast<int>(cached->second.size())))
                        cached->second.clear();
                }
            } else if (!coverJob.valid() && api && !job.valid()) {
                const auto id = target.first;
                coverJob = std::async(std::launch::async, [this, id, key, hero] {
                    std::vector<char> bytes;
                    try { bytes = api->request("getCoverArt", {{"id", id}, {"size", hero ? "768" : "256"}}, salt(), 512 * 1024); }
                    catch (const std::exception&) {}
                    return std::make_pair(key, std::move(bytes));
                });
            }
        }
    }
    void refreshLibrary() {
        if (job.valid()) return;
        preserveFocus = reload;
        if (mode == LibraryMode::Home) loadHome(true);
        else if (mode == LibraryMode::Favorites) loadFavorites(favoriteSongs);
        else if (mode == LibraryMode::Artists) { artistCatalog.loaded = false; loadArtists(offset); }
        else if (mode == LibraryMode::Songs) { songCatalog.loaded = false; loadSongs(offset); }
        else if (mode == LibraryMode::ArtistAlbums) { Entry entry; entry.id = selectedArtistId; entry.title = selectedArtist; loadArtist(entry); }
        else if (mode == LibraryMode::AlbumTracks) loadAlbum(openedAlbum);
        else if (mode == LibraryMode::Playlists) loadPlaylists(true);
        else if (mode == LibraryMode::PlaylistTracks) loadPlaylist(openedPlaylist);
        else { searchCacheValid = false; loadAlbums(offset); }
    }
    void updateNavigation() {
        const bool busy = job.valid();
        for (auto view : {homeNav, playlistsNav, reload, favorites, favoriteKind, albumsNav, artistsNav, songsNav, searchButton, clearSearch, orderButton, viewToggle, shuffleButton})
            if (view) enable(view, !busy);
        homeNav->setStyle(mode == LibraryMode::Home ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
        albumsNav->setStyle(mode == LibraryMode::Albums || (mode == LibraryMode::AlbumTracks && returnMode != LibraryMode::ArtistAlbums) ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
        artistsNav->setStyle(mode == LibraryMode::Artists || mode == LibraryMode::ArtistAlbums || (mode == LibraryMode::AlbumTracks && returnMode == LibraryMode::ArtistAlbums) ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
        playlistsNav->setStyle(mode == LibraryMode::Playlists || mode == LibraryMode::PlaylistTracks ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
        songsNav->setStyle(mode == LibraryMode::Songs ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
        favorites->setStyle(mode == LibraryMode::Favorites ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
        lyricsNav->setStyle(lyricsView ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
        const bool albumView = mode == LibraryMode::Albums;
        const bool collection = isCollection(mode);
        searchButton->setVisibility(collection ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        orderButton->setVisibility(collection ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        clearSearch->setVisibility(collection && !collectionQuery().empty() ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        viewToggle->setVisibility(mode != LibraryMode::Home ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        if (collection) orderButton->setText(brls::getStr(orderCaption()));
        if (mode != LibraryMode::Home) {
            const char* captions[] = {"app/view_list", "app/view_grid", "app/view_coverflow"};
            const int view = static_cast<int>(libraryView());
            viewToggle->setText(brls::getStr(captions[view]) + u8" ›");
            viewToggle->updateActionHint(brls::BUTTON_A, brls::getStr(captions[(view + 1) % 3]));
        }
        favoriteKind->setVisibility(mode == LibraryMode::Favorites ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        favoriteKind->setText(brls::getStr(favoriteSongs ? "app/nav_favorite_albums" : "app/nav_favorite_songs"));
        shuffleButton->setVisibility(mode == LibraryMode::Favorites && favoriteSongs ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        enable(shuffleButton, !busy && !favoriteTracks.empty());
        const bool home = mode == LibraryMode::Home;
        back->setVisibility(home ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        enable(back, !busy && !home);
        const bool paged = mode != LibraryMode::AlbumTracks && mode != LibraryMode::PlaylistTracks && !home;
        pagination->setVisibility(paged ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        size_t total = 0;
        bool known = true;
        if (mode == LibraryMode::Playlists) total = playlists.size();
        else if (mode == LibraryMode::Favorites) total = favoriteSongs ? favoriteTracks.size() : favoriteAlbums.size();
        else if (mode == LibraryMode::ArtistAlbums) total = artistAlbums.size();
        else if (mode == LibraryMode::Artists || mode == LibraryMode::Songs) total = catalog(mode).entries.size();
        else if (albumView && !albumQuery.empty()) total = searchAlbums.size();
        else known = false;
        pageIndicator->setText(known ? brls::getStr("app/page_of", offset / 50 + 1, std::max<size_t>(1, (total + 49) / 50))
            : brls::getStr("app/page_number", offset / 50 + 1));
        const bool ready = (mode != LibraryMode::Artists && mode != LibraryMode::Songs) || catalog(mode).loaded;
        enable(pagePrevious, !busy && ready && paged && offset > 0);
        enable(pageNext, !busy && ready && paged && (known ? static_cast<size_t>(offset) + 50 < total : displayEntries.size() == 50));
    }
    void pageLibrary(int delta) {
        if (job.valid() || mode == LibraryMode::AlbumTracks || mode == LibraryMode::PlaylistTracks || mode == LibraryMode::Home) return;
        const int page = offset + delta;
        if (page < 0) return;
        preserveFocus = delta < 0 ? pagePrevious : pageNext;
        if (mode == LibraryMode::Playlists) showPlaylistPage(page);
        else if (mode == LibraryMode::Favorites) showFavoritePage(favoriteSongs, page);
        else if (mode == LibraryMode::Artists) loadArtists(page);
        else if (mode == LibraryMode::Songs) loadSongs(page);
        else if (mode == LibraryMode::ArtistAlbums) showArtistPage(page);
        else loadAlbums(page);
    }
    void showFavoritePage(bool songs, int page) {
        const auto& all = songs ? favoriteTracks : favoriteAlbums;
        if (page < 0 || (page > 0 && static_cast<size_t>(page) >= all.size())) return;
        mode = LibraryMode::Favorites; favoritesMode = true; favoriteSongs = songs; offset = page;
        const auto end = std::min(all.size(), static_cast<size_t>(page) + 50);
        std::vector<Entry> entries(all.begin() + page, all.begin() + end);
        if (!songs) cachedAlbums = entries;
        heading->setText(songs ? brls::getStr("app/favorite_songs") : brls::getStr("app/favorite_albums"));
        libraryInfo->setText(brls::getStr("app/favorite_page", page / 50 + 1));
        show(entries, !songs, page);
        libraryInfo->setText(brls::getStr("app/favorite_page", page / 50 + 1));
        if (entries.empty()) status->setText(brls::getStr("app/no_favorites"));
    }

    brls::Label* label(brls::Box* box, const std::string& text, float size, float height) {
        auto result = new brls::Label();
        result->setText(text);
        result->setFontSize(size);
        result->setHeight(height);
        result->setShrink(0);
        result->setSingleLine(true);
        box->addView(result);
        return result;
    }
    void enable(brls::Button* view, bool enabled) {
        view->setState(enabled ? brls::ButtonState::ENABLED : brls::ButtonState::DISABLED);
        view->setActionAvailable(brls::BUTTON_A, enabled);
    }
    void refreshPlayer() {
        const bool busy = job.valid();
        const bool playing = music != nullptr;
        waveform.setState(!playing ? WaveformState::Stopped : Mix_PausedMusic() ? WaveformState::Paused : WaveformState::Playing);
        enable(previous, !busy && !queue.empty() && current > 0);
        enable(next, !busy && !queue.empty() && current + 1 < queue.size());
        enable(pause, playing);
        enable(stopButton, playing && !busy);
        enable(lyricsButton, !queue.empty() && !busy);
        enable(lyricsNav, !queue.empty() && !busy);
        pause->setText(playing && !Mix_PausedMusic() ? u8"\ue034" : u8"\ue037");
        pause->updateActionHint(brls::BUTTON_A, brls::getStr(playing && !Mix_PausedMusic() ? "app/pause" : "app/play"));
        playerState->setText(playing ? (Mix_PausedMusic() ? brls::getStr("app/paused") : brls::getStr("app/playing")) : brls::getStr("app/ready"));
        if (playing) playerState->setText(playerState->getFullText() + " · " + brls::getStr("app/playback_mp3", StreamBitRate));
        sourceInfo->setText(queue.empty() ? "" : (audioDetails(queue[current]).empty() ? brls::getStr("app/audio_source_unknown") : brls::getStr("app/audio_source", audioDetails(queue[current]))));
        if (!queue.empty()) {
            trackTitle->setText(queue[current].title);
            trackArtist->setText(artist(queue[current]));
            queueInfo->setText(brls::getStr("app/queue_position", current + 1, queue.size()));
        }
        for (const auto& row : trackRows) {
            const bool active = playing && row.first == queue[current].id;
            row.second->setLineLeft(active ? 4 : 0);
            row.second->setBackgroundColor(active ? panel : nvgRGBA(0, 0, 0, 0));
        }
        lastSecond = -1;
    }
    void togglePause() {
        if (!music) return;
        if (Mix_PausedMusic()) { Mix_ResumeMusic(); clock.resume(); }
        else { Mix_PauseMusic(); clock.pause(); }
        refreshPlayer();
    }
    void goBack() {
        if (job.valid()) return;
        if (mode == LibraryMode::PlaylistTracks) showPlaylistPage(playlistParentOffset);
        else if (mode == LibraryMode::AlbumTracks) {
            if (returnMode == LibraryMode::Home) showHome();
            else if (returnMode == LibraryMode::ArtistAlbums) showArtistPage(returnOffset);
            else if (returnMode == LibraryMode::Favorites) showFavoritePage(false, returnOffset);
            else { offset = returnOffset; mode = LibraryMode::Albums; favoritesMode = false; show(cachedAlbums, true, offset); }
        } else if (mode == LibraryMode::ArtistAlbums) loadArtists(artistParentOffset);
        else if (mode != LibraryMode::Home) loadHome();
    }

    brls::Button* button(brls::Box* box, const std::string& title, std::function<void()> action) {
        auto result = new brls::Button();
        result->applyXMLAttribute("font", "music");
        result->setText(title);
        result->setHeight(42);
        result->setPadding(8, 14, 8, 14);
        result->setShrink(0);
        result->setMarginRight(8);
        result->setCornerRadius(8);
        result->registerClickAction([action](brls::View*) { action(); return true; });
        box->addView(result);
        return result;
    }
    void launch(std::function<std::function<void()>()> task, const std::string& message) {
        if (job.valid()) { status->setText(brls::getStr("app/wait")); return; }
        status->setText(message);
        status->setTextColor(muted);
        job = std::async(std::launch::async, std::move(task));
        updateNavigation();
        refreshPlayer();
    }
    void show(const std::vector<Entry>& entries, bool albums, int page = 0, bool startAtTop = false) {
        const bool foreground = !lyricsScreen && !settingsScreen && !loginScreen;
        if (foreground) brls::Application::giveFocus(preserveFocus ? preserveFocus : reload);
        displayEntries = entries;
        trackRows.clear();
        coverTargets.clear();
        lyricsView = false;
        viewingAlbums = albums;
        updateNavigation();
        const bool grid = gridView();
        auto list = new brls::Box();
        list->setAxis(brls::Axis::COLUMN);
        // ScrollingFrame detaches its content and only sets a maximum width.
        list->setWidth(scroll->getWidth());
        list->setPadding(8, 12, 12, 8);
        auto tracks = std::make_shared<const std::vector<Entry>>(entries);
        auto activate = [this, tracks](size_t i) {
            if (job.valid() || i >= tracks->size()) return;
            const auto entry = (*tracks)[i];
            if (entry.kind == EntryKind::Artist) loadArtist(entry);
            else if (entry.kind == EntryKind::Album) { selectedAlbum = entry.id; loadAlbum(entry); }
            else if (entry.kind == EntryKind::Playlist) loadPlaylist(entry);
            else { queue = *tracks; play(i); }
        };
        brls::View* focus = nullptr;
        const bool flow = libraryView() == LibraryView::CoverFlow;
        if (flow) {
            const auto slot = libraryViewSlot(mode, favoriteSongs);
            size_t selected = 0;
            if (!startAtTop) for (size_t i = 0; i < entries.size(); ++i)
                if (entries[i].id == flowSelections[slot]) { selected = i; break; }
            auto view = new CoverFlow(tracks, selected, scroll->getHeight() - 20,
                [this](auto targets) { coverTargets = std::move(targets); },
                [this, slot, tracks](size_t index) { flowSelections[slot] = (*tracks)[index].id; }, activate);
            view->setCustomNavigationRoute(brls::FocusDirection::UP, viewToggle);
            view->setCustomNavigationRoute(brls::FocusDirection::DOWN,
                mode == LibraryMode::AlbumTracks || mode == LibraryMode::PlaylistTracks ? previous : pagePrevious);
            list->addView(view); focus = entries.empty() ? nullptr : view;
        }
        std::vector<brls::Box*> cards;
        brls::Box* gridRow = nullptr;
        const int columns = preferences.columns;
        const float cardWidth = albumCardWidth(scroll->getWidth(), columns);
        const float artSize = albumArtSize(scroll->getWidth(), scroll->getHeight(), columns);
        for (size_t i = 0; !flow && i < entries.size(); ++i) {
            const auto entry = entries[i];
            auto row = new brls::Box();
            row->setAxis(grid ? brls::Axis::COLUMN : brls::Axis::ROW);
            row->setHeight(grid ? artSize + 86 : 76);
            row->setShrink(0);
            row->setPadding(10, 14, 10, 14);
            row->setMarginBottom(10);
            row->setCornerRadius(12);
            row->setHighlightCornerRadius(12);
            row->setBackgroundColor(nvgRGBA(110, 115, 165, 28));
            row->setFocusable(true);
            row->setLineColor(accent);
            if (grid) {
                row->setPadding(10);
                if (i % columns == 0) {
                    gridRow = new brls::Box(brls::Axis::ROW);
                    list->addView(gridRow);
                }
                row->setWidth(cardWidth);
                row->setMarginRight(i % columns == static_cast<size_t>(columns - 1) ? 0 : AlbumGap);
                auto art = cover(row, artSize);
                art->setAlignSelf(brls::AlignSelf::CENTER);
                art->setMarginRight(0);
                art->setMarginBottom(8);
                coverTargets.push_back({entry.coverArt, art});
                cards.push_back(row);
            }
            if (!grid && entry.kind != EntryKind::Song) coverTargets.push_back({entry.coverArt, cover(row, 56)});
            if (!grid && entry.kind == EntryKind::Song) {
                auto number = label(row, std::to_string(offset + i + 1), 19, 56);
                number->setTextColor(muted);
                number->setWidth(64);
                number->setMarginRight(14);
                number->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
                if (mode == LibraryMode::Favorites) coverTargets.push_back({entry.coverArt, cover(row, 56)});
            }
            auto text = new brls::Box();
            text->setAxis(brls::Axis::COLUMN);
            if (!grid) { text->setGrow(1); text->setWidth(0); text->setShrink(1); }
            label(text, entry.title, grid ? (columns == 6 ? 16 : columns == 5 ? 18 : 20) : 23, 28);
            label(text, entry.kind == EntryKind::Artist ? brls::getStr("app/view_albums") : entry.kind == EntryKind::Playlist ? brls::getStr("app/playlist_tracks", entry.songCount) : artist(entry), 17, 24)->setTextColor(muted);
            row->addView(text);
            if (!grid && entry.duration > 0) {
                auto duration = label(row, playbackTime(entry.duration), 18, 56);
                duration->setWidth(70);
                duration->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
                duration->setTextColor(muted);
            }
            row->registerClickAction([activate, i](brls::View*) { activate(i); return true; });
            (grid ? gridRow : list)->addView(row);
            if (entry.kind == EntryKind::Song) trackRows.push_back({entry.id, row});
            if (!focus || (!startAtTop && ((entry.kind == EntryKind::Album && entry.id == selectedAlbum) || (entry.kind == EntryKind::Artist && entry.title == selectedArtist) || (entry.kind == EntryKind::Playlist && entry.id == openedPlaylist.id) || (entry.kind == EntryKind::Song && music && entry.id == queue[current].id)))) focus = row;
        }
        for (size_t i = 0; i < cards.size(); ++i) {
            const int up = albumVerticalNeighbor(i, cards.size(), -1, columns);
            const int down = albumVerticalNeighbor(i, cards.size(), 1, columns);
            if (up >= 0) cards[i]->setCustomNavigationRoute(brls::FocusDirection::UP, cards[up]);
            if (down >= 0) cards[i]->setCustomNavigationRoute(brls::FocusDirection::DOWN, cards[down]);
        }
        assert(!startAtTop || entries.empty() || focus == list->getDefaultFocus());
        scroll->setContentView(list);
        if (albums && mode == LibraryMode::Albums) {
            heading->setText(favoritesMode ? brls::getStr("app/favorite_albums") : brls::getStr("app/your_albums"));
            libraryInfo->setText(favoritesMode ? brls::getStr("app/starred") : brls::getStr("app/album_page", page / 50 + 1));
        }
        updateNavigation();
        status->setText(entries.empty() ? brls::getStr("app/empty_library")
            : brls::getStr(albums ? (entries.size() == 1 ? "app/album_count_one" : "app/album_count") : (entries.size() == 1 ? "app/track_count_one" : "app/track_count"), entries.size()));
        if (mode == LibraryMode::Playlists) status->setText(brls::getStr(entries.empty() ? "app/no_playlists" : "app/playlist_count", entries.size()));
        if (mode == LibraryMode::Artists && !entries.empty()) status->setText(brls::getStr("app/artist_count", entries.size()));
        if (isCollection(mode) && !collectionQuery().empty() && entries.empty()) status->setText(brls::getStr("app/no_search_results"));
        status->setTextColor(muted);
        refreshPlayer();
        if (foreground) {
            if (preserveFocus) brls::Application::giveFocus(preserveFocus);
            else if (focus) brls::Application::giveFocus(focus);
        }
        preserveFocus = nullptr;
        checkLayout = true;
    }
    void showHome() {
        const bool foreground = !lyricsScreen && !settingsScreen && !loginScreen;
        if (foreground) brls::Application::giveFocus(preserveFocus ? preserveFocus : homeNav);
        mode = LibraryMode::Home; offset = 0; favoritesMode = false; favoriteSongs = false; lyricsView = false;
        viewingAlbums = false; trackRows.clear(); coverTargets.clear(); displayEntries.clear();
        updateNavigation();
        heading->setText(brls::getStr("app/home"));
        libraryInfo->setText(brls::getStr("app/home_hint"));
        auto content = new brls::Box(brls::Axis::COLUMN);
        content->setWidth(scroll->getWidth()); content->setPadding(8);
        std::array<std::vector<brls::Box*>, 5> cards;
        for (size_t section = 0; section < homeSections.size(); ++section) {
            const auto& data = homeSections[section];
            auto title = label(content, brls::getStr(data.title), 26, 38);
            title->setMarginTop(section ? 14 : 0);
            if (data.entries.empty()) {
                auto empty = label(content, brls::getStr(data.unavailable ? "app/home_unavailable" : "app/home_empty"), 18, 60);
                empty->setTextColor(muted); continue;
            }
            auto rail = new HomeRail(data.entries.size()); content->addView(rail);
            auto tracks = std::make_shared<const std::vector<Entry>>(data.entries);
            for (size_t item = 0; item < data.entries.size(); ++item) {
                const auto entry = data.entries[item];
                auto card = new brls::Box(brls::Axis::COLUMN);
                card->setWidth(HomeCardWidth); card->setHeight(240); card->setShrink(0); card->setPadding(8);
                card->setMarginRight(item + 1 < data.entries.size() ? HomeCardGap : 0);
                card->setCornerRadius(12); card->setHighlightCornerRadius(12);
                card->setBackgroundColor(nvgRGBA(110, 115, 165, 28)); card->setFocusable(true); card->setLineColor(accent);
                auto art = cover(card, HomeCardWidth - 16); art->setMarginRight(0); art->setMarginBottom(8);
                coverTargets.push_back({entry.coverArt, art});
                label(card, entry.title, 18, 24);
                label(card, artist(entry), 15, 20)->setTextColor(muted);
                card->registerClickAction([this, entry, tracks, section, item](brls::View*) {
                    if (job.valid()) return true;
                    homeSelectedRow = section; homeSelectedItem = item;
                    if (entry.kind == EntryKind::Album) loadAlbum(entry);
                    else { queue = *tracks; play(item); }
                    return true;
                });
                rail->addCard(card); cards[section].push_back(card);
                if (entry.kind == EntryKind::Song) { trackRows.push_back({entry.id, card}); displayEntries.push_back(entry); }
            }
        }
        for (size_t section = 0; section < cards.size(); ++section) {
            for (size_t item = 0; item < cards[section].size(); ++item) {
                auto card = cards[section][item];
                card->setCustomNavigationRoute(brls::FocusDirection::LEFT, item ? cards[section][item - 1] : homeNav);
                if (item + 1 < cards[section].size()) card->setCustomNavigationRoute(brls::FocusDirection::RIGHT, cards[section][item + 1]);
                for (int direction : {-1, 1}) {
                    int nextSection = static_cast<int>(section) + direction;
                    while (nextSection >= 0 && nextSection < static_cast<int>(cards.size()) && cards[nextSection].empty()) nextSection += direction;
                    if (nextSection >= 0 && nextSection < static_cast<int>(cards.size())) {
                        auto target = cards[nextSection][std::min(item, cards[nextSection].size() - 1)];
                        card->setCustomNavigationRoute(direction < 0 ? brls::FocusDirection::UP : brls::FocusDirection::DOWN, target);
                    } else if (direction < 0) card->setCustomNavigationRoute(brls::FocusDirection::UP, reload);
                }
            }
        }
        scroll->setContentView(content);
        updateNavigation(); refreshPlayer();
        const auto unavailable = std::count_if(homeSections.begin(), homeSections.end(), [](const auto& row) { return row.unavailable; });
        status->setText(brls::getStr(unavailable ? "app/home_partial" : "app/home_controls"));
        status->setTextColor(muted);
        if (foreground) {
            if (preserveFocus) brls::Application::giveFocus(preserveFocus);
            else if (homeSelectedRow < cards.size() && !cards[homeSelectedRow].empty())
                brls::Application::giveFocus(cards[homeSelectedRow][std::min(homeSelectedItem, cards[homeSelectedRow].size() - 1)]);
            else brls::Application::giveFocus(homeNav);
        }
        preserveFocus = nullptr; checkLayout = true;
    }
    void loadHome(bool refresh = false) {
        if (job.valid()) return;
        if (!api) { configure(); if (!api) return; }
        if (homeLoaded && !refresh) { showHome(); return; }
        launch([this] {
            auto rows = fetchHome(*api, salt);
            return [this, rows] { homeSections = rows; homeLoaded = true; showHome(); };
        }, brls::getStr("app/loading_home"));
    }
    void showAlbumPage(const std::vector<Entry>& all, int page) {
        if (page < 0 || (page > 0 && static_cast<size_t>(page) >= all.size())) page = 0;
        const auto end = std::min(all.size(), static_cast<size_t>(page) + 50);
        cachedAlbums.assign(all.begin() + page, all.begin() + end);
        offset = page; mode = LibraryMode::Albums; favoritesMode = false; favoriteSongs = false;
        show(cachedAlbums, true, page);
        if (!albumQuery.empty()) libraryInfo->setText(brls::getStr("app/search_results", albumQuery, page / 50 + 1));
    }
    void loadAlbums(int page) {
        if (job.valid()) return;
        if (!api) { configure(); if (!api) return; }
        if (!albumQuery.empty() && searchCacheValid) { showAlbumPage(searchAlbums, page); return; }
        const auto query = albumQuery;
        const auto order = albumOrder;
        launch([this, page, query, order] {
            if (!query.empty()) {
                std::vector<Entry> all;
                for (int start = 0;; start += 500) {
                    auto body = Api::response(api->request("search3", {{"query", query}, {"artistCount", "0"}, {"songCount", "0"}, {"albumCount", "500"}, {"albumOffset", std::to_string(start)}}, salt()));
                    auto part = Api::entries(body.get(), "searchResult3", "album");
                    if (all.size() + part.size() > 10000) throw ApiError("response_limit");
                    all.insert(all.end(), part.begin(), part.end());
                    if (part.size() < 500) break;
                }
                sortAlbums(all, order);
                return std::function<void()>([this, all, page] { searchAlbums = all; searchCacheValid = true; showAlbumPage(searchAlbums, page); });
            }
            const char* types[] = {"alphabeticalByName", "alphabeticalByArtist", "newest"};
            auto body = Api::response(api->request("getAlbumList2", {{"type", types[order]}, {"size", "50"}, {"offset", std::to_string(page)}}, salt()));
            auto entries = Api::entries(body.get(), "albumList2", "album");
            return std::function<void()>([this, entries, page] { offset = page; mode = LibraryMode::Albums; favoritesMode = false; favoriteSongs = false; cachedAlbums = entries; show(entries, true, page); });
        }, brls::getStr("app/loading_albums"));
    }
    Catalog& catalog(LibraryMode target) { return target == LibraryMode::Artists ? artistCatalog : songCatalog; }
    std::string& collectionQuery() { return mode == LibraryMode::Albums ? albumQuery : catalog(mode).query; }
    int& collectionOrder() { return mode == LibraryMode::Albums ? albumOrder : catalog(mode).order; }
    LibraryView& libraryView() { return libraryViews[libraryViewSlot(mode, favoriteSongs)]; }
    bool gridView() const { return libraryViews[libraryViewSlot(mode, favoriteSongs)] == LibraryView::Grid; }
    const char* orderCaption() {
        if (mode == LibraryMode::Artists) return artistCatalog.order ? "app/order_name_desc" : "app/order_name_asc";
        const char* albums[] = {"app/order_name", "app/order_artist", "app/order_newest"};
        const char* songs[] = {"app/order_title", "app/order_artist", "app/order_newest"};
        return mode == LibraryMode::Songs ? songs[songCatalog.order] : albums[albumOrder];
    }
    void showCollectionPage(LibraryMode target, int page) {
        auto& data = catalog(target);
        if (page < 0 || (page > 0 && static_cast<size_t>(page) >= data.entries.size())) page = 0;
        const auto end = std::min(data.entries.size(), static_cast<size_t>(page) + 50);
        std::vector<Entry> entries(data.entries.begin() + page, data.entries.begin() + end);
        mode = target; favoritesMode = false; offset = page;
        heading->setText(brls::getStr(target == LibraryMode::Artists ? "app/artists" : "app/songs"));
        libraryInfo->setText(data.query.empty() ? brls::getStr("app/collection_count", data.entries.size())
            : brls::getStr("app/search_results", data.query, page / 50 + 1));
        show(entries, false, page);
    }
    void loadCollection(LibraryMode target, int page) {
        if (job.valid()) return;
        if (!api) { configure(); if (!api) return; }
        auto& data = catalog(target);
        if (data.loaded) { showCollectionPage(target, page); return; }
        if (mode != target) {
            auto anchor = preserveFocus; mode = target; favoritesMode = false; offset = page;
            heading->setText(brls::getStr(target == LibraryMode::Artists ? "app/artists" : "app/songs"));
            libraryInfo->setText(data.query.empty() ? brls::getStr("app/collection_hint") : brls::getStr("app/search_results", data.query, page / 50 + 1));
            show({}, false, page); preserveFocus = anchor;
        }
        const auto query = data.query; const auto order = data.order;
        launch([this, target, page, query, order] {
            const auto kind = target == LibraryMode::Artists ? EntryKind::Artist : EntryKind::Song;
            auto entries = fetchCatalog(*api, kind, query, salt); sortEntries(entries, kind, order);
            return [this, target, page, entries = std::move(entries)]() mutable {
                auto& data = catalog(target); data.entries = std::move(entries); data.loaded = true;
                showCollectionPage(target, page);
            };
        }, brls::getStr(target == LibraryMode::Artists ? "app/loading_artists" : "app/loading_songs"));
    }
    void loadArtists(int page) { loadCollection(LibraryMode::Artists, page); }
    void loadSongs(int page) { loadCollection(LibraryMode::Songs, page); }
    void reloadCollection(int page) {
        if (mode == LibraryMode::Albums) loadAlbums(page);
        else loadCollection(mode, page);
    }
    void redrawCollection() {
        if (mode == LibraryMode::Artists || mode == LibraryMode::Songs) showCollectionPage(mode, offset);
        else {
            show(displayEntries, viewingAlbums, offset);
            if (mode == LibraryMode::Albums && !albumQuery.empty())
                libraryInfo->setText(brls::getStr("app/search_results", albumQuery, offset / 50 + 1));
        }
    }
    void clearCollectionSearch() {
        if (job.valid() || !isCollection(mode)) return;
        collectionQuery().clear();
        if (mode == LibraryMode::Albums) searchCacheValid = false; else catalog(mode).loaded = false;
        preserveFocus = searchButton; reloadCollection(0);
    }
    void toggleCollectionView() {
        if (job.valid() || mode == LibraryMode::Home) return;
        libraryView() = static_cast<LibraryView>((static_cast<int>(libraryView()) + 1) % 3); preserveFocus = viewToggle;
        if (isCollection(mode) && mode != LibraryMode::Albums && !catalog(mode).loaded) reloadCollection(offset); else redrawCollection();
    }
    void showArtistPage(int page) {
        if (page < 0 || (page > 0 && static_cast<size_t>(page) >= artistAlbums.size())) return;
        const auto end = std::min(artistAlbums.size(), static_cast<size_t>(page) + 50);
        std::vector<Entry> entries(artistAlbums.begin() + page, artistAlbums.begin() + end);
        mode = LibraryMode::ArtistAlbums; favoritesMode = false; offset = page;
        heading->setText(selectedArtist); libraryInfo->setText(brls::getStr("app/album_page", page / 50 + 1));
        show(entries, true, page);
    }
    void loadArtist(const Entry& entry) {
        if (mode == LibraryMode::Artists) artistParentOffset = offset;
        const int page = mode == LibraryMode::ArtistAlbums ? offset : 0;
        launch([this, entry, page] {
            auto body = Api::response(api->request("getArtist", {{"id", entry.id}}, salt()));
            auto albums = Api::entries(body.get(), "artist", "album");
            return [this, entry, albums, page] { selectedArtist = entry.title; selectedArtistId = entry.id; artistAlbums = albums; showArtistPage(page < static_cast<int>(albums.size()) ? page : 0); };
        }, brls::getStr("app/opening_album", entry.title));
    }
    void loadFavorites(bool songs) {
        if (job.valid()) return;
        if (!api) { configure(); if (!api) return; }
        launch([this, songs] {
            auto body = Api::response(api->request("getStarred2", {}, salt()));
            auto albums = Api::entries(body.get(), "starred2", "album");
            auto tracks = Api::entries(body.get(), "starred2", "song");
            return [this, songs, albums, tracks] {
                favoriteAlbums = albums; favoriteTracks = tracks;
                showFavoritePage(songs, 0);
            };
        }, brls::getStr("app/loading_favorites"));
    }
    void loadAlbum(const Entry& album) {
        if (mode != LibraryMode::AlbumTracks) { returnMode = mode; returnOffset = offset; }
        openedAlbum = album;
        launch([this, album] {
            auto body = Api::response(api->request("getAlbum", {{"id", album.id}}, salt()));
            auto tracks = Api::entries(body.get(), "album", "song");
            for (auto& track : tracks) if (track.coverArt.empty()) track.coverArt = album.coverArt;
            return [this, tracks, album] {
                mode = LibraryMode::AlbumTracks; offset = 0;
                heading->setText(album.title);
                libraryInfo->setText(artist(album));
                show(tracks, false, 0, true);
            };
        }, brls::getStr("app/opening_album", album.title));
    }
    void showPlaylistPage(int page) {
        if (page < 0 || (page > 0 && static_cast<size_t>(page) >= playlists.size())) page = 0;
        const auto end = std::min(playlists.size(), static_cast<size_t>(page) + 50);
        std::vector<Entry> entries(playlists.begin() + page, playlists.begin() + end);
        mode = LibraryMode::Playlists; favoritesMode = false; offset = page;
        heading->setText(brls::getStr("app/playlists")); libraryInfo->setText(brls::getStr("app/playlist_count", playlists.size()));
        show(entries, false, page);
    }
    void loadPlaylists(bool refresh = false) {
        if (job.valid()) return;
        if (!api) { configure(); if (!api) return; }
        if (playlistsLoaded && !refresh) { showPlaylistPage(0); return; }
        const int page = mode == LibraryMode::Playlists ? offset : 0;
        launch([this, page] {
            auto body = Api::response(api->request("getPlaylists", {}, salt()));
            auto entries = Api::entries(body.get(), "playlists", "playlist"); sortEntries(entries, EntryKind::Playlist, 0);
            return [this, page, entries = std::move(entries)]() mutable { playlists = std::move(entries); playlistsLoaded = true; showPlaylistPage(page); };
        }, brls::getStr("app/loading_playlists"));
    }
    void loadPlaylist(const Entry& playlist) {
        if (job.valid()) return;
        if (mode != LibraryMode::PlaylistTracks) playlistParentOffset = offset;
        openedPlaylist = playlist;
        launch([this, playlist] {
            auto body = Api::response(api->request("getPlaylist", {{"id", playlist.id}}, salt(), 8 * 1024 * 1024));
            auto tracks = Api::entries(body.get(), "playlist", "entry");
            for (auto& track : tracks) if (track.coverArt.empty()) track.coverArt = playlist.coverArt;
            return [this, playlist, tracks = std::move(tracks)] {
                mode = LibraryMode::PlaylistTracks; favoritesMode = false; offset = 0;
                heading->setText(playlist.title); libraryInfo->setText(brls::getStr("app/playlist_tracks", tracks.size())); show(tracks, false, 0, true);
            };
        }, brls::getStr("app/opening_playlist", playlist.title));
    }
    void loadLyrics() {
        if (job.valid() || queue.empty()) return;
        const auto track = queue[current];
        if (!lyricsScreen) {
            lyricsScreen = new LyricsScreen([this] { togglePause(); },
                [this] { if (current > 0) play(current - 1); }, [this] { play(current + 1); },
                [this] { lyricsScreen = nullptr; lyricsView = false; reloadLyrics = false; updateNavigation(); refreshPlayer(); });
            lyricsView = true;
            brls::Application::pushActivity(lyricsScreen);
            lyricsScreen->focusLyrics();
        }
        lyricsScreen->setTrack(track);
        launch([this, track] {
            SongLyrics lines;
            try { auto body = Api::response(api->request("getLyricsBySongId", {{"id", track.id}}, salt(), 256 * 1024)); lines = Api::lyrics(body.get()); }
            catch (const std::exception&) {} // Older servers may only support artist/title lookup.
            if (lines.empty()) {
                auto body = Api::response(api->request("getLyrics", {{"artist", track.artist}, {"title", track.title}}, salt(), 256 * 1024));
                lines = Api::lyrics(body.get());
            }
            return [this, track, lines] {
                if (lyricsScreen && queue[current].id == track.id) lyricsScreen->setLyrics(lines);
            };
        }, brls::getStr("app/loading_lyrics"));
    }
    void stop() {
        waveform.setState(WaveformState::Stopped);
        if (music) { Mix_HaltMusic(); Mix_FreeMusic(music); music = nullptr; }
        audio.clear();
        clock.reset();
    }
    void play(size_t index) {
        if (!audioReady) { status->setText(brls::getStr("app/audio_error")); return; }
        if (index >= queue.size() || job.valid()) return;
        stop();
        suppressPlayback = false;
        current = index;
        const auto track = queue[index];
        if (lyricsScreen) lyricsScreen->setTrack(track);
        if (playerCoverId != track.coverArt) {
            playerCover->clearImage();
            playerCoverId = track.coverArt;
        }
        const bool cacheEnabled = preferences.cacheSongs;
        launch([this, track, cacheEnabled] {
            // shortcut: buffer one MP3 up to 64 MiB; use a ring buffer for long recordings.
            std::vector<char> bytes;
            bool cacheFailed = false;
            SongCache cache;
            if (cacheEnabled) { try { bytes = cache.load(*api, track.id); } catch (...) { cacheFailed = true; } }
            if (bytes.empty()) {
                bytes = api->request("stream", {{"id", track.id}, {"format", "mp3"}, {"maxBitRate", std::to_string(StreamBitRate)}}, salt(), 64 * 1024 * 1024);
                if (cacheEnabled) { try { cache.save(*api, track.id, bytes); } catch (...) { cacheFailed = true; } }
            }
            return [this, track, cacheEnabled, cacheFailed, bytes = std::move(bytes)]() mutable {
                if (suppressPlayback) { refreshPlayer(); return; }
                audio = std::move(bytes);
                auto rw = SDL_RWFromConstMem(audio.data(), static_cast<int>(audio.size()));
                if (!rw) throw std::runtime_error(brls::getStr("app/playback_error", SDL_GetError()));
                music = Mix_LoadMUS_RW(rw, 1);
                if (!music || Mix_PlayMusic(music, 0) < 0) {
                    const std::string error = Mix_GetError();
                    if (cacheEnabled) { try { SongCache().erase(*api, track.id); } catch (...) {} }
                    stop();
                    throw std::runtime_error(brls::getStr("app/playback_error", error));
                }
                clock.resume();
                if (lyricsScreen) reloadLyrics = true;
                status->setText(brls::getStr(cacheFailed ? "app/playback_cache_error" : "app/playback_started"));
                refreshPlayer();
            };
        }, brls::getStr("app/preparing_track", track.title));
        playerState->setText(brls::getStr("app/buffering"));
        trackTitle->setText(track.title);
        trackArtist->setText(artist(track));
    }
    void shuffleFavorites() {
        if (job.valid() || favoriteTracks.empty()) return;
        u32 seed; randomGet(&seed, sizeof(seed)); std::mt19937 generator(seed);
        queue = shuffledFavorites(favoriteTracks, generator); play(0);
    }
    void promptSearch() {
        if (job.valid() || !isCollection(mode)) return;
        SwkbdConfig keyboard;
        if (R_FAILED(swkbdCreate(&keyboard, 0))) { status->setText(brls::getStr("app/search_keyboard_error")); return; }
        swkbdConfigMakePresetDefault(&keyboard);
        swkbdConfigSetHeaderText(&keyboard, brls::getStr("app/search").c_str());
        swkbdConfigSetInitialText(&keyboard, collectionQuery().c_str());
        swkbdConfigSetStringLenMax(&keyboard, 128);
        char value[513]{};
        const auto result = swkbdShow(&keyboard, value, sizeof(value));
        swkbdClose(&keyboard);
        if (R_SUCCEEDED(result)) {
            collectionQuery() = value;
            if (mode == LibraryMode::Albums) searchCacheValid = false; else catalog(mode).loaded = false;
            preserveFocus = searchButton; reloadCollection(0);
        }
    }
    void cycleOrder() {
        if (job.valid() || !isCollection(mode)) return;
        auto& order = collectionOrder(); order = (order + 1) % (mode == LibraryMode::Artists ? 2 : 3);
        preserveFocus = orderButton;
        if (mode == LibraryMode::Albums) { searchCacheValid = false; loadAlbums(0); }
        else {
            auto& data = catalog(mode);
            if (!data.loaded) { reloadCollection(0); return; }
            sortEntries(data.entries, mode == LibraryMode::Artists ? EntryKind::Artist : EntryKind::Song, data.order);
            showCollectionPage(mode, 0);
        }
    }
    void applySidebar() {
        const bool collapsed = preferences.sidebarCollapsed;
        sidebar->setWidth(collapsed ? 80 : 232); sidebar->setPadding(collapsed ? 10 : 14);
        brandTitle->setVisibility(collapsed ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        tagline->setVisibility(brls::Visibility::GONE);
        collapseButton->setText(collapsed ? u8"\ue5d2" : u8"\ue5c4");
        collapseButton->updateActionHint(brls::BUTTON_A, brls::getStr("app/collapse_menu"));
        for (const auto& item : menuLabels) {
            item.first->setText(item.second.first + (collapsed ? "" : "  " + brls::getStr(item.second.second)));
            item.first->setJustifyContent(collapsed ? brls::JustifyContent::CENTER : brls::JustifyContent::FLEX_START);
            item.first->applyXMLAttribute("textHorizontalAlign", collapsed ? "center" : "left");
        }
        clockLabel->setVisibility(preferences.showClock ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        needsRerender = true;
    }
    void toggleSidebar() {
        const auto before = preferences;
        preferences.sidebarCollapsed = !preferences.sidebarCollapsed;
        try { saveSettings(preferences); preserveFocus = collapseButton; applySidebar(); }
        catch (...) { preferences = before; status->setText(brls::getStr("app/settings_save_error")); }
    }
    void applyRemote() {
        if (!preferences.remoteEnabled || !api) { remote.stop(); remoteError.clear(); return; }
        if (remote.running()) return;
        try {
            if (R_FAILED(nifmInitialize(NifmServiceType_User))) throw std::runtime_error("remote_network");
            u32 address = 0; auto result = nifmGetCurrentIpAddress(&address); nifmExit();
            if (R_FAILED(result)) throw std::runtime_error("remote_network");
            in_addr ip{}; ip.s_addr = address; char text[16]{};
            if (!inet_ntop(AF_INET, &ip, text, sizeof(text))) throw std::runtime_error("remote_network");
            remote.start(salt(), text, "romfs:/remote"); remoteError.clear();
        } catch (const std::exception& error) { remoteError = error.what(); }
    }
    std::string remoteInfo() {
        if (!api) return brls::getStr("app/logout_remote_off");
        if (!preferences.remoteEnabled) return brls::getStr("app/remote_off");
        if (!remote.running()) return brls::getStr("app/" + (remoteError.empty() ? std::string("remote_start_error") : remoteError));
        return brls::getStr("app/remote_info", remote.url(), remote.pairingCode());
    }
    void openSettings() {
        if (settingsScreen) return;
        settingsScreen = new SettingsScreen([this] { applyRemote(); needsRerender = true; }, [this] { return remoteInfo(); }, [this] { return remote.running() ? pairingUrl(remote.url(), remote.pairingCode()) : std::string(); },
            [this] { settingsScreen = nullptr; preserveFocus = settingsNav; needsRerender = true; },
            [this] { removeSavedLogin(); logoutRequested = true; }, api ? api->serverUrl() : "", api ? api->userName() : "");
        brls::Application::pushActivity(settingsScreen);
    }
    void handleRemote(const RemoteCommand& command) {
        remoteNotice.clear();
        if (command.action == "toggle") { togglePause(); return; }
        if (command.action == "stop") { suppressPlayback = true; stop(); refreshPlayer(); return; }
        if (command.action == "previous") { if (current > 0) play(current - 1); return; }
        if (command.action == "next") { play(current + 1); return; }
        if (!api) { configure(); if (!api) return; }
        if (command.action == "play") {
            for (const auto* source : {&remoteResults, &displayEntries, &favoriteTracks, &queue}) {
                const auto found = std::find_if(source->begin(), source->end(), [&](const Entry& entry) { return entry.kind == EntryKind::Song && entry.id == command.value; });
                if (found != source->end()) {
                    if (source != &queue) queue = *source;
                    const auto selected = std::find_if(queue.begin(), queue.end(), [&](const Entry& entry) { return entry.id == command.value; });
                    play(static_cast<size_t>(std::distance(queue.begin(), selected))); return;
                }
            }
            remoteNotice = brls::getStr("app/remote_unknown_song"); return;
        }
        if (command.action == "search") {
            const auto query = command.value;
            launch([this, query] {
                auto body = Api::response(api->request("search3", {{"query", query}, {"artistCount", "0"}, {"albumCount", "0"}, {"songCount", "50"}}, salt()));
                auto tracks = Api::entries(body.get(), "searchResult3", "song");
                return [this, tracks] { remoteResults = tracks; remoteResultsSet = true; remoteView = brls::getStr("app/songs"); };
            }, brls::getStr("app/loading_songs"));
        } else if (command.action == "favorites" || command.action == "shuffle") {
            const bool shuffle = command.action == "shuffle";
            launch([this, shuffle] {
                auto body = Api::response(api->request("getStarred2", {}, salt()));
                auto tracks = Api::entries(body.get(), "starred2", "song");
                return [this, tracks, shuffle] {
                    favoriteTracks = tracks; remoteResults.assign(tracks.begin(), tracks.begin() + std::min<size_t>(50, tracks.size()));
                    remoteResultsSet = true; remoteView = brls::getStr("app/favorite_songs");
                    if (shuffle) shuffleFavorites();
                };
            }, brls::getStr("app/loading_favorites"));
        }
    }
    void publishRemote() {
        if (!remote.running()) return;
        const auto now = std::chrono::steady_clock::now();
        if (now - remotePublished < std::chrono::milliseconds(500)) return;
        remotePublished = now;
        auto root = jsonOwner(json_object());
        json_object_set_new(root.get(), "busy", json_boolean(job.valid()));
        json_object_set_new(root.get(), "playing", json_boolean(music != nullptr));
        json_object_set_new(root.get(), "paused", json_boolean(music && Mix_PausedMusic()));
        json_object_set_new(root.get(), "view", json_string((remoteResultsSet ? remoteView : heading->getFullText()).c_str()));
        json_object_set_new(root.get(), "notice", json_string(remoteNotice.c_str()));
        auto labels = json_object();
        for (const auto* key : {"previous", "next", "stop", "search", "favorite_songs", "shuffle_all", "play", "pause", "playing", "paused", "stopped", "choose_music"})
            json_object_set_new(labels, key, json_string(brls::getStr(std::string("app/") + key).c_str()));
        json_object_set_new(root.get(), "labels", labels);
        if (!queue.empty()) {
            json_object_set_new(root.get(), "current", json_pack("{s:s,s:s,s:s}", "id", queue[current].id.c_str(), "title", queue[current].title.c_str(), "artist", artist(queue[current]).c_str()));
            const auto clockText = playbackTime(clock.seconds()) + " / " + playbackTime(queue[current].duration);
            json_object_set_new(root.get(), "time", json_string(clockText.c_str()));
        }
        const auto& source = remoteResultsSet ? remoteResults : displayEntries;
        auto songs = json_array();
        for (const auto& entry : source) if (entry.kind == EntryKind::Song)
            json_array_append_new(songs, json_pack("{s:s,s:s,s:s}", "id", entry.id.c_str(), "title", entry.title.c_str(), "artist", artist(entry).c_str()));
        json_object_set_new(root.get(), "songs", songs);
        char* encoded = json_dumps(root.get(), JSON_COMPACT);
        if (encoded) { remote.publish(encoded); std::free(encoded); }
    }
    void logout() {
        logoutRequested = false;
        remote.stop(); suppressPlayback = true; stop();
        if (api) api->cancel();
        if (job.valid()) { job.wait(); try { job.get(); } catch (...) {} }
        if (coverJob.valid()) { coverJob.wait(); try { coverJob.get(); } catch (...) {} }
        api.reset();
        queue.clear(); current = 0;
        homeSections = {}; homeLoaded = false; homeSelectedRow = homeSelectedItem = 0;
        favoriteAlbums.clear(); favoriteTracks.clear(); cachedAlbums.clear();
        playlists.clear(); playlistsLoaded = false; openedPlaylist = {}; playlistParentOffset = 0;
        searchAlbums.clear(); artistAlbums.clear(); albumQuery.clear(); artistCatalog = {}; songCatalog = {}; artistParentOffset = 0; selectedArtistId.clear(); openedAlbum = {}; selectedArtist.clear(); selectedAlbum.clear();
        searchCacheValid = false; albumOrder = 0; returnMode = LibraryMode::Home; returnOffset = 0;
        remoteResults.clear(); remoteResultsSet = false; remoteView.clear(); remoteNotice.clear(); remoteError.clear();
        coverCache.clear(); playerCover->clearImage(); playerCoverId.clear();
        restoreAlbums = false; reloadLyrics = false;
        if (settingsScreen) brls::Application::popActivity();
        mode = LibraryMode::Home; favoritesMode = false; favoriteSongs = false;
        preserveFocus = reload;
        show({}, false);
        heading->setText(brls::getStr("app/signed_out"));
        libraryInfo->setText(brls::getStr("app/logout_hint"));
        status->setText(brls::getStr("app/login_required"));
        trackTitle->setText(brls::getStr("app/choose_music"));
        trackArtist->setText(brls::getStr("app/choose_track"));
        queueInfo->setText(brls::getStr("app/no_track"));
        sourceInfo->setText("");
        playerState->setText(brls::getStr("app/signed_out"));
        orderButton->setText(brls::getStr("app/order_name"));
        needsRerender = false; loginRequested = true;
    }
    void openLogin() {
        if (loginScreen) return;
        loginRequested = false;
        loginScreen = new LoginScreen(salt, [this] { loginSavedRequested = true; },
            [this] { loginScreen = nullptr; preserveFocus = homeNav; needsRerender = true; });
        brls::Application::pushActivity(loginScreen);
    }
    void configure() {
        try {
            auto config = loadLoginConfig();
            if (!config) { api.reset(); loginRequested = true; status->setText(brls::getStr("app/login_required")); return; }
            api = std::make_unique<Api>(config.get()); loginRequested = false;
            applyRemote();
        } catch (...) { api.reset(); loginRequested = true; status->setText(brls::getStr("app/login_config_error")); }
    }

public:
    ~App() override { if (loginScreen) loginScreen->closed = {}; if (lyricsScreen) lyricsScreen->closed = {}; if (settingsScreen) settingsScreen->closed = {}; shutdown(); }
    brls::View* createContentView() override {
        accent = nvgRGB(116, 220, 255);
        muted = nvgRGB(181, 190, 215);
        panel = nvgRGBA(73, 69, 116, 120);
        auto root = new MusicSurface();
        root->setPadding(20, 24, 10, 24);
        auto body = new brls::Box(brls::Axis::ROW);
        body->setHeight(0);
        body->setGrow(1);

        sidebar = new brls::Box(brls::Axis::COLUMN);
        sidebar->setWidth(232); sidebar->setShrink(0); sidebar->setPadding(16);
        sidebar->setMarginRight(24); sidebar->setCornerRadius(18);
        sidebar->setBackgroundColor(nvgRGBA(82, 91, 139, 28));
        clockLabel = label(sidebar, clockText(), 15, 24);
        clockLabel->setTextColor(muted);
        auto brand = new brls::Box(brls::Axis::ROW); brand->setHeight(44); brand->setShrink(0);
        auto logo = new brls::Image(); logo->setDimensions(34, 34); logo->setMarginRight(8);
        logo->setImageFromRes("img/navidrome-logo.png"); brand->addView(logo);
        brandTitle = label(brand, "Navidrome NX", 19, 38); sidebar->addView(brand);
        tagline = label(sidebar, brls::getStr("app/tagline"), 12, 20); tagline->setTextColor(muted);
        collapseButton = button(sidebar, u8"\ue5c4", [this] { toggleSidebar(); });
        collapseButton->setHeight(36); collapseButton->setMarginRight(0);
        auto nav = new brls::Box(brls::Axis::COLUMN); nav->setMarginTop(6);
        homeNav = button(nav, "", [this] { if (!job.valid()) { preserveFocus = homeNav; loadHome(); } });
        artistsNav = button(nav, "", [this] { if (!job.valid()) { preserveFocus = artistsNav; loadArtists(0); } });
        albumsNav = button(nav, "", [this] { if (!job.valid()) { preserveFocus = albumsNav; loadAlbums(0); } });
        songsNav = button(nav, "", [this] { if (!job.valid()) { preserveFocus = songsNav; loadSongs(0); } });
        playlistsNav = button(nav, "", [this] { if (!job.valid()) { preserveFocus = playlistsNav; loadPlaylists(); } });
        favorites = button(nav, "", [this] { if (!job.valid()) { preserveFocus = favorites; loadFavorites(true); } });
        lyricsNav = button(nav, "", [this] { loadLyrics(); });
        settingsNav = button(nav, "", [this] { openSettings(); });
        menuLabels = {
            {homeNav, {u8"\ue88a", "app/home"}},
            {artistsNav, {u8"\ue7fd", "app/artists"}}, {albumsNav, {u8"\ue019", "app/albums"}},
            {playlistsNav, {u8"\ue05f", "app/playlists"}}, {songsNav, {u8"\ue03d", "app/songs"}}, {favorites, {u8"\ue87d", "app/nav_favorite_songs"}},
            {lyricsNav, {u8"\ue244", "app/lyrics"}}, {settingsNav, {u8"\ue8b8", "app/settings"}},
        };
        for (const auto& item : menuLabels) {
            item.first->setJustifyContent(brls::JustifyContent::FLEX_START);
            item.first->applyXMLAttribute("textHorizontalAlign", "left");
            item.first->applyXMLAttribute("fontSize", "16"); item.first->setPadding(6);
            item.first->setHeight(36); item.first->setMarginBottom(4); item.first->setMarginRight(0);
            item.first->setCornerRadius(12); item.first->setHighlightCornerRadius(12);
            item.first->updateActionHint(brls::BUTTON_A, brls::getStr(item.second.second));
        }
        sidebar->addView(nav); sidebar->addView(new brls::Padding());
        body->addView(sidebar);

        library = new brls::Box(brls::Axis::COLUMN);
        library->setWidth(0);
        library->setGrow(1);
        auto libraryHeader = new brls::Box(brls::Axis::ROW); libraryHeader->setHeight(44); libraryHeader->setShrink(0);
        heading = label(libraryHeader, brls::getStr("app/home"), 32, 40); heading->setWidth(0); heading->setGrow(1);
        reload = button(libraryHeader, u8"\ue5d5", [this] { refreshLibrary(); });
        reload->setWidth(44); reload->setPadding(6); reload->setMarginRight(0); reload->updateActionHint(brls::BUTTON_A, brls::getStr("app/refresh"));
        library->addView(libraryHeader);
        libraryInfo = label(library, brls::getStr("app/collection_hint"), 18, 26);
        libraryInfo->setTextColor(muted);
        auto toolbar = new brls::Box(brls::Axis::ROW);
        toolbar->setMarginTop(8); toolbar->setMarginBottom(6);
        back = button(toolbar, brls::getStr("app/back"), [this] { goBack(); });
        searchButton = button(toolbar, brls::getStr("app/search"), [this] { promptSearch(); });
        clearSearch = button(toolbar, brls::getStr("app/clear_search"), [this] { clearCollectionSearch(); });
        orderButton = button(toolbar, brls::getStr("app/order_name"), [this] { cycleOrder(); });
        viewToggle = button(toolbar, u8"\ue8f0", [this] { toggleCollectionView(); }); viewToggle->setWidth(180); viewToggle->setPadding(6); viewToggle->applyXMLAttribute("fontSize", "16");
        favoriteKind = button(toolbar, brls::getStr("app/nav_favorite_albums"), [this] { if (!job.valid()) { preserveFocus = favoriteKind; showFavoritePage(!favoriteSongs, 0); } });
        shuffleButton = button(toolbar, brls::getStr("app/shuffle_all"), [this] { shuffleFavorites(); });
        library->addView(toolbar);
        status = label(library, brls::getStr("app/connecting"), 16, 40);
        status->setSingleLine(false);
        status->setTextColor(muted);
        scroll = new brls::ScrollingFrame();
        scroll->setHeight(0);
        scroll->setGrow(1);
        library->addView(scroll);
        pagination = new brls::Box(brls::Axis::ROW); pagination->setHeight(46); pagination->setShrink(0); pagination->setMarginTop(8);
        pagination->setJustifyContent(brls::JustifyContent::CENTER);
        pagePrevious = button(pagination, u8"\ue5cb", [this] { pageLibrary(-50); }); pagePrevious->setWidth(52); pagePrevious->setPadding(6);
        pagePrevious->updateActionHint(brls::BUTTON_A, brls::getStr("app/previous_page"));
        pageIndicator = label(pagination, "", 16, 42); pageIndicator->setWidth(180); pageIndicator->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        pageNext = button(pagination, u8"\ue5cc", [this] { pageLibrary(50); }); pageNext->setWidth(52); pageNext->setPadding(6); pageNext->setMarginRight(0);
        pageNext->updateActionHint(brls::BUTTON_A, brls::getStr("app/next_page")); library->addView(pagination);
        body->addView(library);
        root->addView(body);

        auto player = new brls::Box(brls::Axis::ROW);
        playerPanel = player;
        player->setHeight(122);
        player->setShrink(0);
        player->setPadding(14);
        player->setMarginTop(14);
        player->setCornerRadius(18);
        player->setBackgroundColor(nvgRGBA(100, 106, 151, 55));
        player->setBorderColor(nvgRGBA(179, 181, 235, 45));
        player->setBorderThickness(1);
        playerCover = cover(player, 80);
        auto metadata = new brls::Box(brls::Axis::COLUMN);
        metadata->setWidth(0);
        metadata->setGrow(1);
        metadata->setMarginRight(20);
        trackTitle = label(metadata, brls::getStr("app/choose_music"), 22, 30);
        trackArtist = label(metadata, brls::getStr("app/choose_track"), 17, 26);
        trackArtist->setTextColor(muted);
        sourceInfo = label(metadata, "", 12, 16); sourceInfo->setTextColor(muted);
        playerState = label(metadata, brls::getStr("app/ready"), 13, 22);
        playerState->setTextColor(accent);
        player->addView(metadata);
        auto transport = new brls::Box(brls::Axis::COLUMN);
        transport->setWidth(450);
        transport->setShrink(0);
        transport->setMarginRight(20);
        auto controls = new brls::Box(brls::Axis::ROW);
        previous = button(controls, u8"\ue045", [this] { if (current > 0) play(current - 1); });
        pause = button(controls, u8"\ue037", [this] { togglePause(); });
        pause->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        next = button(controls, u8"\ue044", [this] { play(current + 1); });
        stopButton = button(controls, u8"\ue047", [this] {
            if (job.valid()) return;
            stop(); refreshPlayer(); playerState->setText(brls::getStr("app/stopped"));
        });
        lyricsButton = button(controls, u8"\ue244", [this] { loadLyrics(); });
        for (auto item : {previous, next, stopButton}) { item->setWidth(70); item->setPadding(6); }
        pause->setWidth(110);
        pause->setPadding(6);
        lyricsButton->setWidth(90);
        lyricsButton->setPadding(6);
        for (auto item : {previous, pause, next, stopButton, lyricsButton}) { item->applyXMLAttribute("glyphCentered", "true"); item->applyXMLAttribute("fontSize", "26"); }
        previous->updateActionHint(brls::BUTTON_A, brls::getStr("app/previous"));
        next->updateActionHint(brls::BUTTON_A, brls::getStr("app/next"));
        stopButton->updateActionHint(brls::BUTTON_A, brls::getStr("app/stop"));
        lyricsButton->updateActionHint(brls::BUTTON_A, brls::getStr("app/lyrics"));
        transport->addView(controls);
        auto timeline = new brls::Box(brls::Axis::ROW);
        timeline->setHeight(5);
        timeline->setShrink(0);
        timeline->setMarginTop(10);
        timeline->setBackgroundColor(nvgRGBA(174, 182, 220, 65));
        progress = new brls::Rectangle(nvgRGB(161, 117, 250));
        progress->setHeight(5);
        progress->setWidthPercentage(0);
        timeline->addView(progress);
        transport->addView(timeline);
        time = label(transport, "0:00", 15, 26);
        time->setTextColor(muted);
        player->addView(transport);
        auto queueSummary = new brls::Box(brls::Axis::COLUMN);
        queueSummary->setWidth(140);
        queueSummary->setShrink(0);
        label(queueSummary, brls::getStr("app/now_playing"), 13, 28)->setTextColor(accent);
        queueInfo = label(queueSummary, brls::getStr("app/no_track"), 15, 26);
        queueInfo->setTextColor(muted);
        queueSummary->addView(new WaveformView(waveform));
        player->addView(queueSummary);
        root->addView(player);
        auto footer = new brls::Box(brls::Axis::ROW);
        footer->setHeight(36);
        footer->setShrink(0);
        footer->setJustifyContent(brls::JustifyContent::FLEX_END);
        label(footer, brls::getStr("app/footer"), 16, 36)->setTextColor(muted);
        root->addView(footer);
        applySidebar();
        updateNavigation();
        refreshPlayer();
        return root;
    }
    void onContentAvailable() override {
        registerAction(brls::getStr("app/back"), brls::BUTTON_B, [this](brls::View*) { goBack(); return true; });
        registerAction(brls::getStr("app/pause"), brls::BUTTON_X, [this](brls::View*) { togglePause(); return true; });
        registerAction(brls::getStr("app/previous"), brls::BUTTON_LB, [this](brls::View*) { if (current > 0) play(current - 1); return true; }, true);
        registerAction(brls::getStr("app/next"), brls::BUTTON_RB, [this](brls::View*) { play(current + 1); return true; }, true);
        registerAction(brls::getStr("app/refresh"), brls::BUTTON_Y, [this](brls::View*) { refreshLibrary(); return true; });
        registerAction(brls::getStr("app/lyrics"), brls::BUTTON_RT, [this](brls::View*) { loadLyrics(); return true; }, true);
        registerAction(brls::getStr("app/collapse_menu"), brls::BUTTON_BACK, [this](brls::View*) { toggleSidebar(); return true; }, true);
        audioReady = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0
            && (Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3)
            && Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 4096) == 0;
        if (audioReady) {
            int frequency = 0, channels = 0; Uint16 format = 0;
            if (Mix_QuerySpec(&frequency, &format, &channels) && format == AUDIO_S16SYS && channels > 0 && channels <= 8) {
                waveform.channels = channels;
                Mix_SetPostMix(AudioWaveform::postMix, &waveform);
            } else brls::Logger::warning("Waveform disabled: unsupported mixer output format");
        }
        configure();
        applyRemote();
        if (api) loadHome();
    }
    void poll() {
        if (logoutRequested) logout();
        if (loginScreen) loginScreen->poll();
        if (loginSavedRequested) {
            loginSavedRequested = false; configure();
            if (api) { brls::Application::popActivity(); homeLoaded = false; loadHome(); }
            else if (loginScreen) loginScreen->showError(brls::getStr("app/login_save_error"));
        }
        if (loginRequested && !loginScreen && !settingsScreen && !lyricsScreen && getContentView()->getAlpha() >= 0.999f) openLogin();
        pollCovers();
        const auto currentClock = preferences.showClock ? clockText() : "";
        if (currentClock != lastClock) { lastClock = currentClock; clockLabel->setText(currentClock); }
        if (settingsScreen) settingsScreen->poll();
        if (needsRerender && !settingsScreen && !lyricsScreen && !loginScreen && !job.valid()) {
            needsRerender = false;
            applySidebar(); needsRerender = false;
            if (mode == LibraryMode::Home && homeLoaded) showHome();
            else if (!displayEntries.empty() || api) redrawCollection();
        }
        if (remote.running() && !job.valid()) { auto command = remote.take(); if (command) handleRemote(*command); }
        publishRemote();
        if (checkLayout) {
            checkLayout = false;
            brls::Logger::info("UI layout: player {}x{}, library list {}x{}", playerPanel->getWidth(), playerPanel->getHeight(), scroll->getWidth(), scroll->getHeight());
            for (const auto& row : trackRows) {
                if (mode != LibraryMode::Home && !gridView() && row.second->getWidth() < scroll->getWidth() - 32)
                    brls::Logger::error("Track row collapsed: width {}", row.second->getWidth());
            }
            for (auto child : playerPanel->getChildren()) {
                if (YGNodeLayoutGetTop(child->getYGNode()) + child->getHeight() > playerPanel->getHeight() + 1)
                    brls::Logger::error("Player content exceeds available height");
            }
        }
        if (restoreAlbums && !job.valid()) {
            restoreAlbums = false;
            if (favoritesMode) showFavoritePage(false, favoriteSongs ? 0 : offset);
            else show(cachedAlbums, true, offset);
        }
        if (job.valid() && job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try { auto complete = job.get(); complete(); }
            catch (const std::exception& error) {
                status->setText(localizedError(error));
                if (lyricsScreen) lyricsScreen->setLyrics({}, localizedError(error));
                remoteNotice = localizedError(error); preserveFocus = nullptr;
                status->setTextColor(brls::Application::getThemeVariant() == brls::ThemeVariant::DARK ? nvgRGB(255, 165, 155) : nvgRGB(165, 35, 28));
            }
            updateNavigation();
            refreshPlayer();
        }
        if (music && !job.valid() && !Mix_PlayingMusic() && !Mix_PausedMusic()) {
            stop();
            if (current + 1 < queue.size()) play(current + 1);
            else { refreshPlayer(); playerState->setText(brls::getStr("app/finished")); }
        }
        if (reloadLyrics && !job.valid() && lyricsScreen) { reloadLyrics = false; loadLyrics(); }
        if (lyricsScreen && !queue.empty()) {
            auto cached = coverCache.find("768:" + queue[current].coverArt);
            lyricsScreen->update(queue[current], clock.milliseconds(), music != nullptr,
                music && Mix_PausedMusic(), cached == coverCache.end() ? nullptr : &cached->second);
        }
        const auto elapsed = clock.seconds();
        if (elapsed != lastSecond) {
            lastSecond = elapsed;
            const auto duration = queue.empty() ? 0 : queue[current].duration;
            time->setText(playbackTime(elapsed) + (duration > 0 ? " / " + playbackTime(duration) : ""));
            progress->setWidthPercentage(duration > 0 ? std::min(100.0, 100.0 * elapsed / duration) : 0);
        }
    }
    void shutdown() {
        remote.stop();
        if (api) api->cancel();
        if (job.valid()) { job.wait(); try { job.get(); } catch (...) {} }
        if (coverJob.valid()) { coverJob.wait(); try { coverJob.get(); } catch (...) {} }
        stop();
        if (audioReady) Mix_SetPostMix(nullptr, nullptr);
        Mix_CloseAudio(); Mix_Quit(); SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
};

int main(int, char**) {
    // Borealis userAppInit/userAppExit own sockets, RomFS, and nxlink stdio.
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return EXIT_FAILURE;
    const bool timeReady = R_SUCCEEDED(timeInitialize());
    try { preferences = loadSettings(); }
    catch (const std::exception& error) { brls::Logger::warning("Unable to load UI settings: {}", error.what()); }
    applyMusicTheme();
    if (!brls::Application::init()) { if (timeReady) timeExit(); curl_global_cleanup(); return EXIT_FAILURE; }
    brls::Application::createWindow("Navidrome NX");
    brls::Application::loadFontFromFile("music", "romfs:/inter/NavidromeMusic.ttf");
    nvgAddFallbackFontId(brls::Application::getNVGContext(), brls::Application::getFont("music"), brls::Application::getFont(brls::FONT_MATERIAL_ICONS));
    brls::Application::loadFontFromFile("lyrics-bold", "romfs:/inter/NavidromeLyrics-Bold.ttf");
    nvgAddFallbackFontId(brls::Application::getNVGContext(), brls::Application::getFont("lyrics-bold"), brls::Application::getFont(brls::FONT_REGULAR));
    brls::Application::setGlobalQuit(true);
    auto app = new App();
    brls::Application::pushActivity(app);
    while (brls::Application::mainLoop()) app->poll();
    curl_global_cleanup();
    if (timeReady) timeExit();
    return EXIT_SUCCESS;
}
