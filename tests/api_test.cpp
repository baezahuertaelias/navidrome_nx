#include "api.hpp"
#include "playback_clock.hpp"
#include "album_grid.hpp"
#include "bubbles.hpp"
#include "home_data.hpp"
#include "audio_details.hpp"
#include "catalog.hpp"
#include <cassert>
#include <iostream>

template<class F> void fails(F fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
int main(int argc, char** argv) {
    for (size_t layer = 0; layer < 3; ++layer) {
        for (float position : {0.0f, 0.25f, 0.5f, 1.0f}) {
            for (double seconds : {0.0, 10.0, 300.0, 86400.0}) {
                const float wave = xmbWaveY(layer, position, seconds, 720);
                assert(std::isfinite(wave) && wave > 250 && wave < 550);
                assert(std::abs(xmbWaveY(layer, position, seconds, 1080) - wave * 1.5f) < 0.001f);
                assert(std::abs(xmbWaveY(layer, position, seconds + 1.0 / 60, 720) - wave) < 1);
            }
            assert(std::abs(xmbWaveY(layer, position, 10, 720) - xmbWaveY(layer, position, 0, 720)) > 0.1f);
        }
    }
    assert(rainbowHue(1) < 1.0f / 120);
    assert(rainbowHue(0) == 0 && rainbowHue(RainbowCycleSeconds) == 0);
    for (double seconds : {0.0, 10.0, 300.0, 86400.0}) {
        const auto glow = backgroundGlow(seconds, 1280, 720);
        const auto next = backgroundGlow(seconds + 1.0 / 60, 1280, 720);
        const auto scaled = backgroundGlow(seconds, 1920, 1080);
        assert(glow.x >= 1280 * 0.15f && glow.x <= 1280 * 0.55f);
        assert(glow.y >= 720 * 0.15f && glow.y <= 720 * 0.55f);
        assert(std::hypot(next.x - glow.x, next.y - glow.y) < 1);
        assert(std::abs(scaled.radius - glow.radius * 1.5f) < 0.001f);
    }
    assert(std::abs(backgroundGlow(10, 1280, 720).x - backgroundGlow(0, 1280, 720).x) > 1);
    assert(rainbowHue(RainbowCycleSeconds / 2) == 0.5f);
    for (double seconds : {0.0, 10.0, RainbowCycleSeconds - 0.01, 300.0, 86400.0}) {
        const float hue = rainbowHue(seconds), next = rainbowHue(seconds + 1.0 / 60);
        assert(hue >= 0 && hue < 1);
        const float distance = std::abs(next - hue);
        assert(std::min(distance, 1 - distance) < 0.001f);
        assert(std::abs(rainbowHue(seconds + RainbowCycleSeconds) - hue) < 0.00001f);
    }
    for (size_t i = 0; i < MaxBubbleCount; ++i) {
        const auto initial = bubbleAt(i, 0, 1280, 720);
        const auto moved = bubbleAt(i, 10, 1280, 720);
        assert(std::hypot(moved.x - initial.x, moved.y - initial.y) > 1);
        for (double second : {0.0, 0.5, 10.0, 300.0, 86400.0}) {
            const auto bubble = bubbleAt(i, second, 1280, 720);
            const auto scaled = bubbleAt(i, second, 1920, 1080);
            assert(std::isfinite(bubble.x) && std::isfinite(bubble.y) && std::isfinite(bubble.radius));
            assert(bubble.radius > 150 && bubble.radius < 300);
            assert(bubble.x > -128 && bubble.x < 1408 && bubble.y > -72 && bubble.y < 792);
            assert(std::abs(scaled.x - bubble.x * 1.5f) < 0.001f);
            assert(std::abs(scaled.radius - bubble.radius * 1.5f) < 0.001f);
        }
        const auto oneFrame = bubbleAt(i, 1.0 / 60, 1280, 720);
        assert(std::hypot(oneFrame.x - initial.x, oneFrame.y - initial.y) < 1);
    }
    for (float width : {976.0f, 1128.0f}) {
        for (int columns : {4, 5, 6}) {
            const auto art = albumArtSize(width, 302, columns);
            assert(art > 100 && art + 86 <= 302 - 14);
            assert(art + 20 <= albumCardWidth(width, columns));
        }
    }
    assert(compareNames("Alpha", "alpha") == 0 && compareNames("a", "B") < 0);
    assert(isCollection(LibraryMode::Artists) && isCollection(LibraryMode::Songs) && !isCollection(LibraryMode::Home));
    assert(albumCardWidth(976) == 230);
    assert(AlbumColumns * albumCardWidth(976) + (AlbumColumns - 1) * AlbumGap + AlbumListPadding == 976);
    assert(albumVerticalNeighbor(0, 10, -1) == -1);
    assert(albumVerticalNeighbor(0, 10, 1) == 4);
    assert(albumVerticalNeighbor(5, 10, 1) == 9);
    assert(albumVerticalNeighbor(6, 10, 1) == -1);
    assert(albumVerticalNeighbor(8, 10, -1) == 4);
    assert(albumVerticalNeighbor(0, 0, 1) == -1);
    assert(playbackTime(0) == "0:00" && playbackTime(65) == "1:05");
    assert(playbackTime(-1) == "0:00" && playbackTime(3600) == "60:00");
    PlaybackClock clock;
    auto start = std::chrono::steady_clock::time_point{};
    clock.resume(start);
    clock.resume(start + std::chrono::seconds(1));
    assert(clock.seconds(start + std::chrono::seconds(10)) == 10);
    assert(clock.milliseconds(start + std::chrono::milliseconds(10500)) == 10500);
    clock.pause(start + std::chrono::seconds(10));
    clock.pause(start + std::chrono::seconds(15));
    assert(clock.seconds(start + std::chrono::seconds(20)) == 10);
    assert(clock.milliseconds(start + std::chrono::seconds(20)) == 10000);
    clock.resume(start + std::chrono::seconds(20));
    assert(clock.seconds(start + std::chrono::seconds(25)) == 15);
    clock.reset();
    assert(clock.seconds(start + std::chrono::seconds(30)) == 0);
    assert(homeRailOffset(0, 0, 976, 30) == 0);
    assert(homeRailOffset(0, 1, 976, 30) == 0);
    assert(homeRailOffset(12, 12, 976, 0) == 0);
    float railOffset = 0;
    for (size_t i = 0; i < 12; ++i) {
        railOffset = homeRailOffset(i, 12, 976, railOffset);
        const auto left = HomeRailPadding + i * (HomeCardWidth + HomeCardGap) - railOffset;
        assert(left >= HomeRailPadding && left + HomeCardWidth <= 976 - HomeRailPadding);
    }
    assert(homeRailOffset(0, 12, 976, railOffset) == 0);
    assert(homeRailOffset(11, 12, 1128, railOffset) < railOffset);
    assert(reflectedPosition(0, 1, 10, 0, 10) == 10);
    assert(reflectedPosition(0, 1, 11, 0, 10) == 9);
    assert(reflectedPosition(0, 1, 20, 0, 10) == 0);
    assert(reflectedPosition(0, -1, 1, 0, 10) == 1);
    for (size_t i = 0; i < MaxBubbleCount; ++i) {
        assert(bubbleAt(i, 0, 1280, 720, 1).radius > bubbleAt(i, 0, 1280, 720, 5).radius);
        assert(bubbleAt(i, 0, 1280, 720, 5).radius > bubbleAt(i, 0, 1280, 720, 10).radius);
        for (size_t count : {size_t{1}, size_t{5}, size_t{10}}) {
            for (double seconds : {0.0, 0.1, 60.0, 86400.0}) {
                const auto bubble = bubbleAt(i, seconds, 1280, 720, count);
                assert(bubble.x - bubble.radius >= -0.001f && bubble.x + bubble.radius <= 1280.001f);
                assert(bubble.y - bubble.radius >= -0.001f && bubble.y + bubble.radius <= 720.001f);
            }
        }
    }
    Entry details; assert(audioDetails(details).empty());
    details.suffix = "mp3"; details.bitRate = 320; details.samplingRate = 48000;
    assert(audioDetails(details) == "MP3 · 320 kbps · 48 kHz");
    details.samplingRate = 22050; assert(audioDetails(details).find("22.05 kHz") != std::string::npos);
    details.suffix = "bad\nformat"; details.bitRate = -1; details.samplingRate = 0;
    assert(audioDetails(details).empty());
    assert(argc == 2);
    assert(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    {
        auto config = jsonOwner(json_pack("{s:s,s:s,s:s}", "server", argv[1], "username", "a user+&", "password", "s3cret"));
        Api api(config.get());
        assert(api.serverUrl() == argv[1] && api.userName() == "a user+&");
        Api::response(api.request("ping", {}, "fixed-salt", 64 * 1024));
        auto artists = fetchCatalog(api, EntryKind::Artist, "dense & /", [] { return "fixed-salt"; });
        assert(artists.size() == 507 && artists.front().kind == EntryKind::Artist);
        sortEntries(artists, EntryKind::Artist, 0); assert(artists.front().id == "artist-506");
        assert(artists[49].title < artists[50].title);
        sortEntries(artists, EntryKind::Artist, 1); assert(artists.front().id == "artist-0");
        auto songs = fetchCatalog(api, EntryKind::Song, "dense & /", [] { return "fixed-salt"; });
        assert(songs.size() == 513 && songs.front().kind == EntryKind::Song);
        sortEntries(songs, EntryKind::Song, 0); assert(songs.front().id == "song-512");
        sortEntries(songs, EntryKind::Song, 1); assert(songs.front().artist == "Artist 0");
        assert(fetchCatalog(api, EntryKind::Artist, "empty", [] { return "fixed-salt"; }).empty());
        fails([&] { fetchCatalog(api, EntryKind::Song, "overflow", [] { return "fixed-salt"; }); });
        auto playlistReply = Api::response(api.request("getPlaylists", {}, "fixed-salt"));
        auto playlistEntries = Api::entries(playlistReply.get(), "playlists", "playlist");
        assert(playlistEntries.size() == 1 && playlistEntries[0].kind == EntryKind::Playlist);
        assert(playlistEntries[0].title == "Evening mix" && playlistEntries[0].songCount == 2);
        auto playlistReply2 = Api::response(api.request("getPlaylist", {{"id", playlistEntries[0].id}}, "fixed-salt"));
        auto playlistTracks = Api::entries(playlistReply2.get(), "playlist", "entry");
        assert(playlistTracks.size() == 2 && playlistTracks[0].kind == EntryKind::Song && playlistTracks[0].id == "first" && playlistTracks[1].id == "second");
        assert(audioDetails(playlistTracks[0]) == "FLAC · 900 kbps");
        assert(Api::entries(playlistReply2.get(), "missing", "entry").empty());
        const auto home = fetchHome(api, [] { return "fixed-salt"; });
        assert(home.size() == 5 && home[0].entries[0].id == "frequent");
        assert(home[0].entries[0].kind == EntryKind::Album);
        assert(home[1].entries[0].id == "favorite-track" && home[1].entries[0].kind == EntryKind::Song);
        assert(home[2].entries.empty() && !home[2].unavailable);
        assert(home[3].entries[0].id == "random");
        assert(home[4].entries.size() == 12 && home[4].entries[0].kind == EntryKind::Song);
        for (const auto& row : home) assert(!row.unavailable);
        const auto partialHome = fetchHome(api, [] { return "partial-salt"; });
        for (size_t i = 0; i < 4; ++i) assert(!partialHome[i].unavailable);
        assert(partialHome[4].unavailable && partialHome[4].entries.empty());
        auto wrongConfig = jsonOwner(json_pack("{s:s,s:s,s:s}", "server", argv[1], "username", "a user+&", "password", "wrong"));
        Api wrong(wrongConfig.get());
        fails([&] { Api::response(wrong.request("ping", {}, "fixed-salt", 64 * 1024)); });
        const auto deniedHome = fetchHome(wrong, [] { return "fixed-salt"; });
        for (const auto& row : deniedHome) assert(row.unavailable && row.entries.empty());
        auto albums = Api::response(api.request("getAlbumList2", {{"type", "alphabeticalByName"}}, "fixed-salt"));
        auto entries = Api::entries(albums.get(), "albumList2", "album");
        assert(entries.size() == 1 && entries[0].id == "id /&" && entries[0].title == "Album" && entries[0].coverArt == "cover /&");
        auto album = Api::response(api.request("getAlbum", {{"id", entries[0].id}}, "fixed-salt"));
        auto tracks = Api::entries(album.get(), "album", "song");
        assert(tracks.size() == 1 && tracks[0].title == "Track" && tracks[0].duration == 185);
        assert(audioDetails(tracks[0]) == "FLAC · 980 kbps · 24-bit · 44.1 kHz · 2 ch");
        assert(entries[0].duration == 0);
        assert(tracks[0].coverArt.empty());
        auto favorites = Api::response(api.request("getStarred2", {}, "fixed-salt"));
        assert(Api::entries(favorites.get(), "starred2", "album")[0].title == "Favorite album");
        assert(Api::entries(favorites.get(), "starred2", "song")[0].title == "Favorite track");
        assert(Api::entries(favorites.get(), "starred2", "artist").empty());
        auto lyrics = Api::response(api.request("getLyricsBySongId", {{"id", "track"}}, "fixed-salt"));
        auto timed = Api::lyrics(lyrics.get());
        assert(timed.lines.size() == 2 && timed.lines[0].text == "First test line" && timed.lines[1].text == "Second test line");
        assert(timed.synced && timed.activeLine(-1) == -1 && timed.activeLine(0) == 0);
        assert(timed.activeLine(1999) == 0 && timed.activeLine(2000) == 1 && timed.activeLine(10000) == 1);
        timed.offsetMs = 100;
        assert(timed.activeLine(99) == -1 && timed.activeLine(100) == 0 && timed.activeLine(2100) == 1);
        timed.offsetMs = -100;
        assert(timed.activeLine(1900) == 1);
        timed.offsetMs = 0;
        timed.lines[1].startMs = 0;
        assert(timed.activeLine(0) == 1);
        auto badTiming = jsonOwner(json_loads(R"({"lyricsList":{"structuredLyrics":[{"synced":true,"line":[{"start":1000,"value":"a"},{"start":0,"value":"b"}]}]}})", 0, nullptr));
        assert(!Api::lyrics(badTiming.get()).synced);
        auto missingTiming = jsonOwner(json_loads(R"({"lyricsList":{"structuredLyrics":[{"synced":true,"line":[{"value":"a"}]}]}})", 0, nullptr));
        assert(!Api::lyrics(missingTiming.get()).synced);
        auto offsetLyrics = jsonOwner(json_loads(R"({"lyricsList":{"structuredLyrics":[{"synced":true,"offset":-100,"line":[{"start":200,"value":"a"}]}]}})", 0, nullptr));
        auto offsetData = Api::lyrics(offsetLyrics.get());
        assert(offsetData.offsetMs == -100 && offsetData.activeLine(99) == -1 && offsetData.activeLine(100) == 0);
        auto emptyLyrics = Api::response(api.request("getLyricsBySongId", {{"id", "empty"}}, "fixed-salt"));
        assert(Api::lyrics(emptyLyrics.get()).empty());
        auto plainLyrics = Api::response(api.request("getLyrics", {{"artist", "Artist"}, {"title", "Track"}}, "fixed-salt"));
        auto plain = Api::lyrics(plainLyrics.get());
        assert(plain.lines.size() == 3 && plain.lines[0].text == "First test line" && plain.lines[1].text.empty() && plain.lines[2].text == "Last test line");
        assert(!plain.synced && plain.activeLine(1000) == -1);
        auto excessiveLyrics = jsonOwner(json_pack("{s:{s:s}}", "lyrics", "value", std::string(2001, '\n').c_str()));
        fails([&] { Api::lyrics(excessiveLyrics.get()); });
        auto art = api.request("getCoverArt", {{"id", entries[0].coverArt}, {"size", "128"}}, "fixed-salt", 512 * 1024);
        assert(art.size() > 8 && art[0] == static_cast<char>(0x89) && art[1] == 'P');
        auto audio = api.request("stream", {{"id", "track"}, {"format", "mp3"}, {"maxBitRate", "192"}}, "fixed-salt");
        assert(std::string(audio.begin(), audio.end()) == "fake mp3");
        auto output = std::unique_ptr<FILE, decltype(&std::fclose)>(std::tmpfile(), std::fclose);
        assert(output);
        assert(api.request("stream", {}, "fixed-salt", 8, output.get()).empty());
        std::rewind(output.get()); char downloaded[9]{};
        assert(std::fread(downloaded, 1, 8, output.get()) == 8 && std::string(downloaded) == "fake mp3");
        fails([&] { api.request("stream", {}, "fixed-salt", 2, output.get()); });
        for (const auto endpoint : {"denied", "malformed", "missingStatus"})
            fails([&] { Api::response(api.request(endpoint, {}, "fixed-salt")); });
        for (const auto endpoint : {"httpError", "redirect"})
            fails([&] { api.request(endpoint, {}, "fixed-salt"); });
        fails([&] { api.request("stream", {}, "fixed-salt", 2); });
        assert(Api::entries(album.get(), "album", "absent").empty());
        json_object_set_new(config.get(), "server", json_string("file:///tmp/secret"));
        fails([&] { Api invalid(config.get()); });
        Transfer buffer{{}, 3};
        char sample[] = "abcd";
        assert(Transfer::write(sample, 1, 3, &buffer) == 3);
        assert(Transfer::write(sample, 1, 1, &buffer) == 0 && buffer.overflow);
        assert(buffer.bytes.size() == 3);
        api.cancel();
        fails([&] { api.request("stream", {}, "fixed-salt"); });
    }
    curl_global_cleanup();
    std::cout << "API checks passed\n";
}
