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
    brls::Box* playerPanel = nullptr;
    brls::Button *favorites = nullptr, *favoriteKind = nullptr, *lyricsButton = nullptr;
    brls::Image* playerCover = nullptr;
    std::vector<std::pair<std::string, brls::Image*>> coverTargets;
    std::map<std::string, std::vector<char>> coverCache;
    std::future<std::pair<std::string, std::vector<char>>> coverJob;
    std::string playerCoverId;
    std::vector<Entry> browsingEntries;
    std::vector<Entry> favoriteAlbums, favoriteTracks;
    std::string browsingTitle, browsingInfo;
    bool favoritesMode = false, favoriteSongs = false, lyricsView = false, restoreBrowser = false;
    std::vector<Entry> cachedAlbums;
    std::vector<std::pair<std::string, brls::Box*>> trackRows;
    std::string selectedAlbum;
    PlaybackClock clock;
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
            if (coverCache.size() >= 64) coverCache.erase(coverCache.begin());
            coverCache[result.first] = std::move(result.second);
        }
        auto targets = coverTargets;
        targets.insert(targets.begin(), {playerCoverId, playerCover});
        for (const auto& target : targets) {
            if (target.first.empty() || target.second->getTexture()) continue;
            if (target.second != playerCover && (target.second->getY() + target.second->getHeight() < scroll->getY()
                || target.second->getY() > scroll->getY() + scroll->getHeight())) continue;
            auto cached = coverCache.find(target.first);
            if (cached != coverCache.end()) {
                if (!cached->second.empty()) {
                    if (!target.second->setImageFromMemory(reinterpret_cast<const unsigned char*>(cached->second.data()), static_cast<int>(cached->second.size())))
                        cached->second.clear();
                }
            } else if (!coverJob.valid() && api && !job.valid()) {
                const auto id = target.first;
                coverJob = std::async(std::launch::async, [this, id] {
                    std::vector<char> bytes;
                    try { bytes = api->request("getCoverArt", {{"id", id}, {"size", "128"}}, salt(), 512 * 1024); }
                    catch (const std::exception&) {} // Missing artwork must not interrupt playback.
                    return std::make_pair(id, std::move(bytes));
                });
            }
        }
    }
    void refreshLibrary() {
        if (job.valid()) return;
        if (favoritesMode) loadFavorites(favoriteSongs);
        else loadAlbums(offset);
    }
    void updateNavigation() {
        const bool busy = job.valid();
        enable(reload, !busy);
        enable(favorites, !busy);
        enable(favoriteKind, !busy);
        favorites->setText(favoritesMode ? "All albums" : "Favorites");
        favoriteKind->setText(favoriteSongs ? "Fav albums" : "Fav songs");
        favoriteKind->setVisibility(favoritesMode ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        enable(back, !busy && (!viewingAlbums || lyricsView));
        const bool paged = !lyricsView && (viewingAlbums || (favoritesMode && favoriteSongs));
        const auto favoriteCount = favoriteSongs ? favoriteTracks.size() : favoriteAlbums.size();
        enable(pagePrevious, !busy && paged && offset > 0);
        enable(pageNext, !busy && paged && (favoritesMode ? static_cast<size_t>(offset) + 50 < favoriteCount : cachedAlbums.size() == 50));
    }
    void pageLibrary(int delta) {
        if (job.valid() || lyricsView || (!viewingAlbums && !(favoritesMode && favoriteSongs))) return;
        const int page = offset + delta;
        if (page < 0) return;
        if (favoritesMode) showFavoritePage(favoriteSongs, page);
        else loadAlbums(page);
    }
    void showFavoritePage(bool songs, int page) {
        const auto& all = songs ? favoriteTracks : favoriteAlbums;
        if (page < 0 || (page > 0 && static_cast<size_t>(page) >= all.size())) return;
        favoritesMode = true; favoriteSongs = songs; offset = page;
        const auto end = std::min(all.size(), static_cast<size_t>(page) + 50);
        std::vector<Entry> entries(all.begin() + page, all.begin() + end);
        if (!songs) cachedAlbums = entries;
        heading->setText(songs ? "Favorite songs" : "Favorite albums");
        libraryInfo->setText("Starred in Navidrome  /  Page " + std::to_string(page / 50 + 1));
        show(entries, !songs, page);
        libraryInfo->setText("Starred in Navidrome  /  Page " + std::to_string(page / 50 + 1));
        browsingInfo = libraryInfo->getFullText();
        if (entries.empty()) status->setText("No favorites yet. Star music in Navidrome, then Refresh.");
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
    }
    void refreshPlayer() {
        const bool busy = job.valid();
        const bool playing = music != nullptr;
        enable(previous, !busy && !queue.empty() && current > 0);
        enable(next, !busy && !queue.empty() && current + 1 < queue.size());
        enable(pause, playing);
        enable(stopButton, playing && !busy);
        enable(lyricsButton, !queue.empty() && !busy);
        pause->setText(playing && !Mix_PausedMusic() ? "Pause" : "Play");
        playerState->setText(playing ? (Mix_PausedMusic() ? "PAUSED" : "PLAYING") : "READY TO LISTEN");
        if (!queue.empty()) {
            trackTitle->setText(queue[current].title);
            trackArtist->setText(queue[current].artist);
            queueInfo->setText("Track " + std::to_string(current + 1) + " of " + std::to_string(queue.size()));
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
        if (lyricsView) restoreBrowser = true;
        else if (favoritesMode && favoriteSongs) loadAlbums(0);
        else if (!viewingAlbums) restoreAlbums = true;
    }

    brls::Button* button(brls::Box* box, const std::string& title, std::function<void()> action) {
        auto result = new brls::Button();
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
        if (job.valid()) { status->setText("Please wait for the current request"); return; }
        status->setText(message);
        status->setTextColor(muted);
        job = std::async(std::launch::async, std::move(task));
        updateNavigation();
        refreshPlayer();
    }
    void show(const std::vector<Entry>& entries, bool albums, int page = 0) {
        brls::Application::giveFocus(reload);
        trackRows.clear();
        coverTargets.clear();
        lyricsView = false;
        viewingAlbums = albums;
        browsingEntries = entries;
        auto list = new brls::Box();
        list->setAxis(brls::Axis::COLUMN);
        // ScrollingFrame detaches its content and only sets a maximum width.
        list->setWidth(scroll->getWidth());
        list->setPadding(8, 12, 12, 8);
        auto tracks = std::make_shared<const std::vector<Entry>>(entries);
        brls::View* focus = nullptr;
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto entry = entries[i];
            auto row = new brls::Box();
            row->setAxis(brls::Axis::ROW);
            row->setHeight(82);
            row->setShrink(0);
            row->setPadding(10, 12, 10, 16);
            row->setMarginBottom(6);
            row->setCornerRadius(8);
            row->setHighlightCornerRadius(8);
            row->setFocusable(true);
            row->setLineColor(accent);
            if (albums) coverTargets.push_back({entry.coverArt, cover(row, 56)});
            if (!albums) {
                auto number = label(row, std::to_string(i + 1), 20, 56);
                number->setTextColor(muted);
                number->setWidth(44);
            }
            auto text = new brls::Box();
            text->setAxis(brls::Axis::COLUMN);
            text->setGrow(1);
            text->setWidth(0);
            text->setShrink(1);
            label(text, entry.title, 23, 32);
            label(text, entry.artist, 18, 26)->setTextColor(muted);
            row->addView(text);
            if (!albums && entry.duration > 0) {
                auto duration = label(row, playbackTime(entry.duration), 18, 56);
                duration->setWidth(70);
                duration->setHorizontalAlign(brls::HorizontalAlign::RIGHT);
                duration->setTextColor(muted);
            }
            row->registerClickAction([this, entry, albums, tracks, i](brls::View*) {
                if (job.valid()) return true;
                if (albums) { selectedAlbum = entry.id; loadAlbum(entry); }
                else { queue = *tracks; play(i); }
                return true;
            });
            list->addView(row);
            if (!albums) trackRows.push_back({entry.id, row});
            if (!focus || (albums && entry.id == selectedAlbum) || (!albums && music && entry.id == queue[current].id)) focus = row;
        }
        scroll->setContentView(list);
        if (albums) {
            heading->setText(favoritesMode ? "Favorite albums" : "Your albums");
            libraryInfo->setText(favoritesMode ? "Starred in Navidrome" : "A–Z  /  Page " + std::to_string(page / 50 + 1));
        }
        updateNavigation();
        status->setText(entries.empty() ? "Nothing here yet. Try Refresh or the previous page."
            : std::to_string(entries.size()) + (albums ? " albums  ·  Select to browse tracks" : " tracks  ·  Select to play the album from here"));
        status->setTextColor(muted);
        refreshPlayer();
        if (focus) brls::Application::giveFocus(focus);
        browsingTitle = heading->getFullText();
        browsingInfo = libraryInfo->getFullText();
        checkLayout = true;
    }
    void loadAlbums(int page) {
        if (!api) { configure(); if (!api) return; }
        launch([this, page] {
            auto body = Api::response(api->request("getAlbumList2", {{"type", "alphabeticalByName"}, {"size", "50"}, {"offset", std::to_string(page)}}, salt()));
            auto entries = Api::entries(body.get(), "albumList2", "album");
            return [this, entries, page] { offset = page; favoritesMode = false; favoriteSongs = false; cachedAlbums = entries; show(entries, true, page); };
        }, "Loading your albums...");
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
        }, "Loading your favorites...");
    }
    void loadAlbum(const Entry& album) {
        launch([this, album] {
            auto body = Api::response(api->request("getAlbum", {{"id", album.id}}, salt()));
            auto tracks = Api::entries(body.get(), "album", "song");
            for (auto& track : tracks) if (track.coverArt.empty()) track.coverArt = album.coverArt;
            return [this, tracks, album] {
                heading->setText(album.title);
                libraryInfo->setText(album.artist);
                show(tracks, false);
            };
        }, "Opening " + album.title + "...");
    }
    void showLyrics(const Entry& track, const std::vector<std::string>& lines) {
        brls::Application::giveFocus(reload);
        trackRows.clear(); coverTargets.clear();
        lyricsView = true;
        auto list = new brls::Box();
        list->setAxis(brls::Axis::COLUMN);
        list->setWidth(scroll->getWidth());
        list->setPadding(12);
        brls::View* focus = nullptr;
        for (const auto& line : lines) {
            auto row = new brls::Box();
            row->setAxis(brls::Axis::COLUMN);
            row->setPadding(10);
            row->setFocusable(true);
            auto text = label(row, line.empty() ? " " : line, 23, brls::View::AUTO);
            text->setSingleLine(false);
            list->addView(row);
            if (!focus) focus = row;
        }
        scroll->setContentView(list);
        heading->setText("Lyrics · " + track.title);
        libraryInfo->setText(track.artist);
        status->setText(lines.empty() ? "No lyrics available for this track on your server."
            : "Up / Down to scroll  ·  B to return to your music");
        updateNavigation();
        if (focus) brls::Application::giveFocus(focus);
    }
    void loadLyrics() {
        if (job.valid() || queue.empty()) return;
        if (lyricsView) { restoreBrowser = true; return; }
        const auto track = queue[current];
        launch([this, track] {
            std::vector<std::string> lines;
            try { auto body = Api::response(api->request("getLyricsBySongId", {{"id", track.id}}, salt(), 256 * 1024)); lines = Api::lyrics(body.get()); }
            catch (const std::exception&) {} // Older servers may only support artist/title lookup.
            if (lines.empty()) {
                auto body = Api::response(api->request("getLyrics", {{"artist", track.artist}, {"title", track.title}}, salt(), 256 * 1024));
                lines = Api::lyrics(body.get());
            }
            return [this, track, lines] { showLyrics(track, lines); };
        }, "Loading lyrics...");
    }
    void stop() {
        if (music) { Mix_HaltMusic(); Mix_FreeMusic(music); music = nullptr; }
        audio.clear();
        clock.reset();
    }
    void play(size_t index) {
        if (!audioReady) { status->setText("Audio initialization failed; restart the app"); return; }
        if (index >= queue.size() || job.valid()) return;
        stop();
        if (lyricsView) restoreBrowser = true;
        current = index;
        const auto track = queue[index];
        if (playerCoverId != track.coverArt) {
            playerCover->clearImage();
            playerCoverId = track.coverArt;
        }
        launch([this, track] {
            // shortcut: buffer one MP3 up to 64 MiB; use a ring buffer for long recordings.
            auto bytes = api->request("stream", {{"id", track.id}, {"format", "mp3"}, {"maxBitRate", "192"}}, salt(), 64 * 1024 * 1024);
            return [this, track, bytes = std::move(bytes)]() mutable {
                audio = std::move(bytes);
                auto rw = SDL_RWFromConstMem(audio.data(), static_cast<int>(audio.size()));
                if (!rw) throw std::runtime_error(SDL_GetError());
                music = Mix_LoadMUS_RW(rw, 1);
                if (!music || Mix_PlayMusic(music, 0) < 0) {
                    const std::string error = Mix_GetError();
                    stop();
                    throw std::runtime_error("Playback failed: " + error);
                }
                clock.resume();
                status->setText("Playing. Browse freely while you listen.");
                refreshPlayer();
            };
        }, "Preparing " + track.title + " for playback...");
        playerState->setText("BUFFERING");
        trackTitle->setText(track.title);
        trackArtist->setText(track.artist);
    }
    void configure() {
        try {
            auto config = jsonOwner(json_load_file("sdmc:/switch/navidrome_nx/config.json", JSON_REJECT_DUPLICATES, nullptr));
            if (!config) throw std::runtime_error("Create /switch/navidrome_nx/config.json on the SD card; see config.example.json");
            api = std::make_unique<Api>(config.get());
        } catch (const std::exception& error) { status->setText(error.what()); }
    }
public:
    ~App() override { shutdown(); }
    brls::View* createContentView() override {
        const bool dark = brls::Application::getThemeVariant() == brls::ThemeVariant::DARK;
        accent = dark ? nvgRGB(80, 225, 195) : nvgRGB(0, 105, 92);
        muted = dark ? nvgRGB(185, 194, 198) : nvgRGB(88, 98, 104);
        panel = dark ? nvgRGB(36, 47, 50) : nvgRGB(222, 235, 233);
        auto frame = new brls::AppletFrame();
        frame->setTitle("Navidrome NX");
        // This Borealis revision ships colored placeholder rectangles in its footer.
        auto footer = static_cast<brls::Box*>(frame->getChildren().back());
        while (!footer->getChildren().empty()) footer->removeView(footer->getChildren().back());
        label(footer, "A Select   B Back   X Pause   L / R Skip   ZR Lyrics   Y Refresh   + Exit", 18, 30)->setTextColor(muted);
        auto box = new brls::Box();
        box->setAxis(brls::Axis::ROW);
        box->setShrink(1);
        box->setPadding(16, 24, 16, 24);
        auto library = new brls::Box();
        library->setAxis(brls::Axis::COLUMN);
        library->setWidth(0);
        library->setGrow(1);
        library->setMarginRight(24);
        heading = label(library, "Your albums", 30, 36);
        libraryInfo = label(library, "Your music, ready when you are", 19, 26);
        libraryInfo->setTextColor(muted);
        auto toolbar = new brls::Box();
        toolbar->setAxis(brls::Axis::ROW);
        toolbar->setMarginTop(8);
        toolbar->setMarginBottom(6);
        back = button(toolbar, "Back", [this] { goBack(); });
        reload = button(toolbar, "Refresh", [this] { refreshLibrary(); });
        favorites = button(toolbar, "Favorites", [this] {
            if (job.valid()) return;
            if (favoritesMode) loadAlbums(0); else loadFavorites(true);
        });
        favoriteKind = button(toolbar, "Fav songs", [this] { if (!job.valid()) showFavoritePage(!favoriteSongs, 0); });
        pagePrevious = button(toolbar, "Page <", [this] { pageLibrary(-50); });
        pageNext = button(toolbar, "Page >", [this] { pageLibrary(50); });
        library->addView(toolbar);
        status = label(library, "Connecting to your library...", 17, 44);
        status->setSingleLine(false);
        status->setTextColor(muted);
        scroll = new brls::ScrollingFrame();
        scroll->setHeight(0);
        scroll->setGrow(1);
        library->addView(scroll);
        box->addView(library);

        auto player = new brls::Box();
        playerPanel = player;
        player->setAxis(brls::Axis::COLUMN);
        player->setWidth(340);
        player->setShrink(0);
        player->setPadding(20);
        player->setCornerRadius(16);
        player->setBackgroundColor(panel);
        auto playerHeader = new brls::Box();
        playerHeader->setAxis(brls::Axis::ROW);
        playerHeader->setHeight(84);
        playerHeader->setShrink(0);
        playerCover = cover(playerHeader, 80);
        auto caption = new brls::Box();
        caption->setAxis(brls::Axis::COLUMN);
        caption->setWidth(0);
        caption->setGrow(1);
        label(caption, "NOW PLAYING", 17, 28)->setTextColor(accent);
        playerState = label(caption, "READY TO LISTEN", 15, 22);
        playerState->setTextColor(muted);
        playerHeader->addView(caption);
        player->addView(playerHeader);
        trackTitle = label(player, "Choose your music", 25, 52);
        trackTitle->setSingleLine(false);
        trackArtist = label(player, "Select an album, then a track", 18, 32);
        trackArtist->setTextColor(muted);
        trackArtist->setSingleLine(false);
        queueInfo = label(player, "Your album plays in order", 17, 22);
        queueInfo->setTextColor(muted);
        auto timeline = new brls::Box();
        timeline->setAxis(brls::Axis::ROW);
        timeline->setHeight(6);
        timeline->setShrink(0);
        timeline->setBackgroundColor(dark ? nvgRGB(75, 90, 92) : nvgRGB(170, 191, 188));
        progress = new brls::Rectangle(accent);
        progress->setHeight(6);
        progress->setWidthPercentage(0);
        timeline->addView(progress);
        player->addView(timeline);
        time = label(player, "0:00", 17, 26);
        time->setTextColor(muted);
        auto controls = new brls::Box();
        controls->setAxis(brls::Axis::ROW);
        previous = button(controls, "Prev", [this] { if (current > 0) play(current - 1); });
        next = button(controls, "Next", [this] { play(current + 1); });
        for (auto control : {previous, next}) { control->setWidth(142); control->setPadding(8); }
        player->addView(controls);
        pause = button(player, "Play", [this] { togglePause(); });
        pause->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        pause->setMarginTop(6);
        auto secondary = new brls::Box();
        secondary->setAxis(brls::Axis::ROW);
        secondary->setMarginTop(6);
        stopButton = button(secondary, "Stop", [this] {
            if (job.valid()) return;
            stop(); refreshPlayer(); playerState->setText("STOPPED");
        });
        lyricsButton = button(secondary, "Lyrics", [this] { loadLyrics(); });
        for (auto control : {stopButton, lyricsButton}) { control->setWidth(142); control->setPadding(8); }
        player->addView(secondary);
        box->addView(player);
        updateNavigation();
        refreshPlayer();
        frame->setContentView(box);
        return frame;
    }
    void onContentAvailable() override {
        registerAction("Albums", brls::BUTTON_B, [this](brls::View*) { goBack(); return true; });
        registerAction("Pause", brls::BUTTON_X, [this](brls::View*) { togglePause(); return true; });
        registerAction("Previous", brls::BUTTON_LB, [this](brls::View*) { if (current > 0) play(current - 1); return true; }, true);
        registerAction("Next", brls::BUTTON_RB, [this](brls::View*) { play(current + 1); return true; }, true);
        registerAction("Refresh", brls::BUTTON_Y, [this](brls::View*) { refreshLibrary(); return true; });
        registerAction("Lyrics", brls::BUTTON_RT, [this](brls::View*) { loadLyrics(); return true; }, true);
        audioReady = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0
            && (Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3)
            && Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 4096) == 0;
        configure();
        if (api) loadAlbums(0);
    }
    void poll() {
        pollCovers();
        if (checkLayout) {
            checkLayout = false;
            brls::Logger::info("UI layout: player {}x{}, library list {}x{}", playerPanel->getWidth(), playerPanel->getHeight(), scroll->getWidth(), scroll->getHeight());
            for (const auto& row : trackRows) {
                if (row.second->getWidth() < scroll->getWidth() - 32)
                    brls::Logger::error("Track row collapsed: width {}", row.second->getWidth());
            }
            for (auto child : playerPanel->getChildren()) {
                if (YGNodeLayoutGetTop(child->getYGNode()) + child->getHeight() > playerPanel->getHeight() + 1)
                    brls::Logger::error("Player content exceeds available height");
            }
        }
        if (restoreBrowser) {
            restoreBrowser = false;
            heading->setText(browsingTitle);
            libraryInfo->setText(browsingInfo);
            show(browsingEntries, viewingAlbums, offset);
        }
        if (restoreAlbums && !job.valid()) {
            restoreAlbums = false;
            if (favoritesMode) showFavoritePage(false, favoriteSongs ? 0 : offset);
            else show(cachedAlbums, true, offset);
        }
        if (job.valid() && job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try { auto complete = job.get(); complete(); }
            catch (const std::exception& error) {
                status->setText(error.what());
                status->setTextColor(brls::Application::getThemeVariant() == brls::ThemeVariant::DARK ? nvgRGB(255, 165, 155) : nvgRGB(165, 35, 28));
            }
            updateNavigation();
            refreshPlayer();
        }
        if (music && !job.valid() && !Mix_PlayingMusic() && !Mix_PausedMusic()) {
            stop();
            if (current + 1 < queue.size()) play(current + 1);
            else { refreshPlayer(); playerState->setText("ALBUM FINISHED"); }
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
        if (api) api->cancel();
        if (job.valid()) { job.wait(); try { job.get(); } catch (...) {} }
        if (coverJob.valid()) { coverJob.wait(); try { coverJob.get(); } catch (...) {} }
        stop();
        Mix_CloseAudio(); Mix_Quit(); SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
};

int main(int, char**) {
    // Borealis userAppInit/userAppExit own sockets, RomFS, and nxlink stdio.
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return EXIT_FAILURE;
    if (!brls::Application::init()) { curl_global_cleanup(); return EXIT_FAILURE; }
    brls::Application::createWindow("Navidrome NX");
    brls::Application::setGlobalQuit(true);
    auto app = new App();
    brls::Application::pushActivity(app);
    while (brls::Application::mainLoop()) app->poll();
    curl_global_cleanup();
    return EXIT_SUCCESS;
}
