// SPDX-License-Identifier: GPL-2.0-or-later
#define TESLA_INIT_IMPL
#include <tesla.hpp>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MAX_DIMENSIONS 256
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <nanovg/stb_image.h>
#include <future>
#include <chrono>
#include <thread>
#include "tune.h"
#include "login.hpp"
#include "library.hpp"
#include "cache.hpp"

namespace {
bool spanish = false, networkReady = false;
const char* tr(const char* en, const char* es) { return spanish ? es : en; }
void check(Result result) {
    if (R_FAILED(result)) { char code[32]; std::snprintf(code, sizeof(code), "IPC 0x%08X", result); throw std::runtime_error(code); }
}
std::string servicePath(const std::string& path) { return path.rfind("sdmc:", 0) == 0 ? path.substr(5) : path; }
std::string mountedPath(const std::string& path) { return path.rfind("/", 0) == 0 ? "sdmc:" + path : path; }
std::vector<unsigned char> cover(const Api& api, const std::string& id) {
    if (id.empty()) return {};
    auto bytes = api.request("getCoverArt", {{"id", id}, {"size", "128"}}, overlay::salt(), 128 * 1024);
    int w = 0, h = 0, channels = 0;
    if (!stbi_info_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), &w, &h, &channels)
        || w < 1 || h < 1 || w > 256 || h > 256) return {};
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> image(stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), &w, &h, &channels, 4), stbi_image_free);
    if (!image) return {};
    std::vector<unsigned char> result(128 * 128 * 4);
    for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x) {
        const auto source = ((y * h / 128) * w + (x * w / 128)) * 4;
        std::copy_n(image.get() + source, 4, result.begin() + (y * 128 + x) * 4);
    }
    return result;
}

class PlayerGui;
PlayerGui* activePlayer = nullptr;
class PlayerGui : public tsl::Gui {
    tsl::elm::List* list = nullptr;
    tsl::elm::ListItem* notice = nullptr;
    std::vector<tsl::elm::Element*> rows;
    std::vector<overlay::Page> history;
    overlay::Page page;
    bool home = true, connected = false, playing = false;
    std::string message, currentPath, requestedArt;
    Entry current;
    TuneCurrentStats stats{};
    std::vector<unsigned char> artwork;
    std::future<std::function<void()>> job;
    std::shared_ptr<Api> activeApi;
    unsigned generation = 0;
    std::atomic<size_t> downloaded{0}, downloading{0};
    int focusDelay = 0;
    size_t restoreIndex = 0, homeFocus = 0;
    std::chrono::steady_clock::time_point nextPoll{};

    bool busy() const { return job.valid(); }
    void status(const std::string& value) { message = value; if (notice) notice->setText(value); }
    void connect() {
        if (connected) return;
        auto rc = tuneInitialize();
        if (R_FAILED(rc)) { status(tr("sys-tune unavailable; install it and reboot", "sys-tune no disponible; instálalo y reinicia")); return; }
        u32 version = 0;
        if (R_FAILED(tuneGetApiVersion(&version)) || version != 4) {
            tuneExit(); status(tr("Requires sys-tune API 4 (2.0.0)", "Requiere sys-tune API 4 (2.0.0)")); return;
        }
        connected = true;
    }
    bool start(std::function<std::function<void()>(std::shared_ptr<Api>)> work) {
        if (busy()) { status(tr("Loading… please wait", "Cargando… espera")); return false; }
        try {
            if (!networkReady) throw std::runtime_error(tr("Network unavailable", "Red no disponible"));
            auto config = loadLoginConfig();
            if (!config) throw std::runtime_error(tr("Sign in using the Navidrome NX app first", "Inicia sesión en Navidrome NX primero"));
            activeApi = std::make_shared<Api>(config.get());
            downloading = 0; downloaded = 0;
            job = std::async(std::launch::async, [api = activeApi, work = std::move(work)] { return work(api); });
            status(tr("Loading…", "Cargando…")); return true;
        } catch (const std::exception& error) { status(error.what()); return false; }
    }
    void rememberFocus() {
        const auto found = std::find(rows.begin(), rows.end(), getFocusedElement());
        if (found != rows.end()) page.focus = std::distance(rows.begin(), found);
    }
    void load(overlay::Page target, bool push) {
        if (busy()) { status(tr("Loading… please wait", "Cargando… espera")); return; }
        rememberFocus();
        if (home) homeFocus = page.focus;
        if (target.items.empty()) target.focus = 1;
        const auto expected = generation;
        start([this, target, push, expected](auto api) {
            auto result = overlay::browse(*api, target);
            return [this, result = std::move(result), push, expected]() mutable {
                if (generation != expected) return;
                if (push && !home) history.push_back(page);
                home = false; page = std::move(result); rebuild(page.focus);
                status(page.items.empty() ? tr("No songs or albums found", "No se encontraron canciones ni álbumes") : tr("Ready", "Listo"));
            };
        });
    }
    std::set<std::string> protectedQueue() {
        std::set<std::string> files;
        u32 count = 0; check(tuneGetPlaylistSize(&count));
        if (count > 2048) throw std::runtime_error("sys-tune queue too large");
        for (u32 i = 0; i < count; ++i) {
            char path[FS_MAX_PATH]{}; check(tuneGetPlaylistItem(i, path, sizeof(path)));
            files.insert(mountedPath(path));
        }
        if (!currentPath.empty()) files.insert(currentPath);
        return files;
    }
    void playPage(size_t selected) {
        connect(); if (!connected || busy() || selected >= page.items.size()) return;
        try {
            auto protectedFiles = protectedQueue();
            auto tracks = page.items;
            const auto expected = generation;
            start([this, tracks, selected, expected, protectedFiles = std::move(protectedFiles)](auto api) mutable {
                std::vector<std::string> paths; downloading = tracks.size();
                for (const auto& track : tracks) { paths.push_back(overlay::cacheTrack(*api, track, protectedFiles)); ++downloaded; }
                // Change the live queue only after every download has completed successfully.
                return [this, paths = std::move(paths), selected, expected] {
                    if (generation != expected) return;
                    check(tunePause()); check(tuneClearQueue()); check(tuneSetShuffleMode(TuneShuffleMode_Off)); check(tuneSetRepeatMode(TuneRepeatMode_Off));
                    for (const auto& path : paths) check(tuneEnqueue(servicePath(path).c_str(), TuneEnqueueType_Back));
                    check(tuneSelect(selected)); check(tunePlay());
                    status(tr("Playing cached page (up to 10 songs)", "Reproduciendo página guardada (hasta 10 canciones)")); nextPoll = {};
                };
            });
            status(tr("Downloading page… keep overlay open", "Descargando página… mantén abierto el overlay"));
        } catch (const std::exception& error) { status(error.what()); }
    }
    void transport(int action) {
        connect(); if (!connected) return;
        try {
            if (action == 0) check(tunePrev());
            else if (action == 1) { bool state = false; check(tuneGetStatus(&state)); check(state ? tunePause() : tunePlay()); }
            else if (action == 2) check(tuneNext());
            else { check(tunePause()); check(tuneClearQueue()); }
            nextPoll = {};
        } catch (const std::exception& error) { connected = false; tuneExit(); status(error.what()); }
    }
    void add(const std::string& title, std::function<void()> action, const std::string& value = "") {
        auto item = new tsl::elm::ListItem(title, value);
        item->setClickListener([action = std::move(action)](u64 keys) { if (!(keys & HidNpadButton_A)) return false; action(); return true; });
        rows.push_back(item); list->addItem(item);
    }
    void rebuild(size_t focus = 0) {
        removeFocus(); list->clear(); rows.clear(); notice = nullptr;
        list->addItem(new tsl::elm::CustomDrawer([this](auto renderer, s32 x, s32 y, s32 w, s32) {
            const tsl::Color white{15,15,15,15}, muted{10,10,12,15}, purple{11,7,15,15};
            if (!artwork.empty()) renderer->drawBitmap(x + (w - 128) / 2, y + 6, 128, 128, artwork.data());
            else {
                renderer->drawRect(x + (w - 128) / 2, y + 6, 128, 128, tsl::Color{3,2,5,15});
                renderer->drawString("♪", false, x + w / 2 - 16, y + 90, 48, purple);
            }
            renderer->drawString(current.title.empty() ? tr("Nothing playing", "Sin reproducción") : current.title, false, x + 8, y + 163, 21, white, w - 16);
            renderer->drawString(current.artist, false, x + 8, y + 188, 17, muted, w - 16);
            renderer->drawRect(x + 8, y + 204, w - 16, 4, tsl::Color{4,4,5,15});
            const float progress = stats.total_frames ? std::min(1.0f, static_cast<float>(stats.current_frame) / stats.total_frames) : 0;
            renderer->drawRect(x + 8, y + 204, static_cast<s32>((w - 16) * progress), 4, purple);
            renderer->drawString(overlay::elapsed(stats.current_frame, stats.sample_rate) + " / " + overlay::elapsed(stats.total_frames, stats.sample_rate) + (playing ? "   ▶" : "   Ⅱ"), false, x + 8, y + 232, 16, muted, w - 16);
        }), 242);
        add(tr("Previous     Play / Pause     Next", "Anterior     Reproducir / Pausa     Siguiente"), [this] { transport(1); }, "L / A / R");
        notice = new tsl::elm::ListItem(message.empty() ? tr("Ready", "Listo") : message);
        list->addItem(notice);
        if (home) {
            add(tr("Favorites", "Favoritos"), [this] { load({overlay::Collection::Favorites, "", tr("Favorites", "Favoritos")}, false); });
            add(tr("Albums", "Álbumes"), [this] { load({overlay::Collection::Albums, "", tr("Albums", "Álbumes")}, false); });
            add(tr("Artists", "Artistas"), [this] { load({overlay::Collection::Artists, "", tr("Artists", "Artistas")}, false); });
            add(tr("Stop and clear queue", "Detener y vaciar cola"), [this] { transport(3); });
        } else {
            list->addItem(new tsl::elm::CategoryHeader(page.title));
            for (size_t i = 0; i < page.items.size(); ++i) {
                const auto entry = page.items[i];
                add(entry.title, [this, entry, i] {
                    if (entry.kind == EntryKind::Song) playPage(i);
                    else load({entry.kind == EntryKind::Artist ? overlay::Collection::ArtistAlbums : overlay::Collection::AlbumSongs, entry.id, entry.title, entry.coverArt}, true);
                }, entry.kind == EntryKind::Song ? "▶" : "›");
            }
            if (page.offset) add(tr("Previous page", "Página anterior"), [this] { auto target = page; target.offset -= overlay::PageSize; target.focus = 1; load(target, false); }, "‹");
            if (page.more) add(tr("Next page", "Página siguiente"), [this] { auto target = page; target.offset += overlay::PageSize; target.focus = 1; load(target, false); }, "›");
            add(tr("Refresh", "Actualizar"), [this] { load(page, false); });
        }
        restoreIndex = std::min(focus, rows.size() - 1); focusDelay = 2; restoreFocus();
    }
public:
    PlayerGui() { activePlayer = this; }
    ~PlayerGui() override { shutdown(); activePlayer = nullptr; }
    void shutdown() {
        if (activeApi) activeApi->cancel();
        if (job.valid()) { job.wait(); job = {}; }
        activeApi.reset();
        if (connected) { tuneExit(); connected = false; }
    }
    tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("Navidrome NX", tr("L previous · A select · R next · X pause", "L anterior · A elegir · R siguiente · X pausa"));
        list = new tsl::elm::List(); frame->setContent(list); connect(); rebuild(); return frame;
    }
    void update() override {
        if (busy() && downloading.load()) status(std::string(tr("Downloading songs: ", "Descargando canciones: ")) + std::to_string(downloaded.load()) + "/" + std::to_string(downloading.load()));
        if (focusDelay && --focusDelay == 0 && !rows.empty()) requestFocus(rows[restoreIndex], tsl::FocusDirection::None, false);
        if (job.valid() && job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try { auto done = job.get(); activeApi.reset(); done(); }
            catch (const std::exception& error) { activeApi.reset(); status(error.what()); }
        }
        const auto now = std::chrono::steady_clock::now();
        if (connected && now >= nextPoll) {
            nextPoll = now + std::chrono::milliseconds(300);
            auto rc = tuneGetStatus(&playing);
            if (R_FAILED(rc)) { connected = false; tuneExit(); status(tr("sys-tune disconnected", "sys-tune desconectado")); return; }
            char path[FS_MAX_PATH]{}; TuneCurrentStats value{};
            if (R_FAILED(tuneGetCurrentQueueItem(path, sizeof(path), &value))) {
                stats = {}; playing = false; currentPath.clear(); current = {}; artwork.clear(); requestedArt.clear();
            }
            else {
                stats = value;
                const auto mounted = mountedPath(path);
                if (currentPath != mounted) {
                    currentPath = mounted; current = overlay::readMetadata(mounted); artwork.clear(); requestedArt.clear();
                    if (current.title.empty()) current.title = tr("sys-tune track", "Canción de sys-tune");
                }
            }
        }
        if (!busy() && !current.coverArt.empty() && requestedArt != current.coverArt) {
            const auto id = current.coverArt, path = currentPath;
            requestedArt = id;
            const auto oldMessage = message;
            if (start([this, id, path](auto api) {
                std::vector<unsigned char> pixels;
                try { pixels = cover(*api, id); } catch (...) {} // Artwork failure must not stop playback.
                return [this, path, pixels = std::move(pixels)]() mutable { if (path == currentPath) artwork = std::move(pixels); };
            })) status(oldMessage);
        }
    }
    bool handleInput(u64 down, u64, const HidTouchState&, HidAnalogStickState, HidAnalogStickState) override {
        if (down & HidNpadButton_L) { transport(0); return true; }
        if (down & HidNpadButton_R) { transport(2); return true; }
        if (down & HidNpadButton_X) { transport(1); return true; }
        if ((down & HidNpadButton_B) && !home) {
            if (busy()) { ++generation; activeApi->cancel(); }
            if (history.empty()) home = true;
            else { page = std::move(history.back()); history.pop_back(); }
            rebuild(home ? homeFocus : page.focus); return true;
        }
        return false;
    }
};
class NavidromeOverlay : public tsl::Overlay {
public:
    void initServices() override {
        u64 language = 0;
        if (R_SUCCEEDED(setInitialize())) {
            if (R_SUCCEEDED(setGetSystemLanguage(&language))) {
            SetLanguage value{};
            spanish = R_SUCCEEDED(setMakeLanguage(language, &value)) && (value == SetLanguage_ES || value == SetLanguage_ES419);
            }
            setExit();
        }
        SocketInitConfig config = *socketGetDefaultInitConfig();
        config.tcp_tx_buf_size = 16 * 1024; config.tcp_rx_buf_size = 32 * 1024;
        config.tcp_tx_buf_max_size = 0; config.tcp_rx_buf_max_size = 0;
        config.num_bsd_sessions = 1;
        networkReady = R_SUCCEEDED(socketInitialize(&config));
        if (networkReady && curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) { socketExit(); networkReady = false; }
    }
    void exitServices() override { if (activePlayer) activePlayer->shutdown(); if (networkReady) { curl_global_cleanup(); socketExit(); } }
    std::unique_ptr<tsl::Gui> loadInitialGui() override { return initially<PlayerGui>(); }
};
}
int main(int argc, char** argv) { return tsl::loop<NavidromeOverlay>(argc, argv); }
