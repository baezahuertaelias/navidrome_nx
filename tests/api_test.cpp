#include "api.hpp"
#include "playback_clock.hpp"
#include <cassert>
#include <iostream>

template<class F> void fails(F fn) {
    bool failed = false;
    try { fn(); } catch (const std::exception&) { failed = true; }
    assert(failed);
}
int main(int argc, char** argv) {
    assert(playbackTime(0) == "0:00" && playbackTime(65) == "1:05");
    assert(playbackTime(-1) == "0:00" && playbackTime(3600) == "60:00");
    PlaybackClock clock;
    auto start = std::chrono::steady_clock::time_point{};
    clock.resume(start);
    clock.resume(start + std::chrono::seconds(1));
    assert(clock.seconds(start + std::chrono::seconds(10)) == 10);
    clock.pause(start + std::chrono::seconds(10));
    clock.pause(start + std::chrono::seconds(15));
    assert(clock.seconds(start + std::chrono::seconds(20)) == 10);
    clock.resume(start + std::chrono::seconds(20));
    assert(clock.seconds(start + std::chrono::seconds(25)) == 15);
    clock.reset();
    assert(clock.seconds(start + std::chrono::seconds(30)) == 0);
    assert(argc == 2);
    assert(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    {
        auto config = jsonOwner(json_pack("{s:s,s:s,s:s}", "server", argv[1], "username", "a user+&", "password", "s3cret"));
        Api api(config.get());
        auto albums = Api::response(api.request("getAlbumList2", {{"type", "alphabeticalByName"}}, "fixed-salt"));
        auto entries = Api::entries(albums.get(), "albumList2", "album");
        assert(entries.size() == 1 && entries[0].id == "id /&" && entries[0].title == "Album" && entries[0].coverArt == "cover /&");
        auto album = Api::response(api.request("getAlbum", {{"id", entries[0].id}}, "fixed-salt"));
        auto tracks = Api::entries(album.get(), "album", "song");
        assert(tracks.size() == 1 && tracks[0].title == "Track" && tracks[0].duration == 185);
        assert(entries[0].duration == 0);
        assert(tracks[0].coverArt.empty());
        auto favorites = Api::response(api.request("getStarred2", {}, "fixed-salt"));
        assert(Api::entries(favorites.get(), "starred2", "album")[0].title == "Favorite album");
        assert(Api::entries(favorites.get(), "starred2", "song")[0].title == "Favorite track");
        assert(Api::entries(favorites.get(), "starred2", "artist").empty());
        auto lyrics = Api::response(api.request("getLyricsBySongId", {{"id", "track"}}, "fixed-salt"));
        assert((Api::lyrics(lyrics.get()) == std::vector<std::string>{"First test line", "Second test line"}));
        auto emptyLyrics = Api::response(api.request("getLyricsBySongId", {{"id", "empty"}}, "fixed-salt"));
        assert(Api::lyrics(emptyLyrics.get()).empty());
        auto plainLyrics = Api::response(api.request("getLyrics", {{"artist", "Artist"}, {"title", "Track"}}, "fixed-salt"));
        assert((Api::lyrics(plainLyrics.get()) == std::vector<std::string>{"First test line", "", "Last test line"}));
        auto excessiveLyrics = jsonOwner(json_pack("{s:{s:s}}", "lyrics", "value", std::string(2001, '\n').c_str()));
        fails([&] { Api::lyrics(excessiveLyrics.get()); });
        auto art = api.request("getCoverArt", {{"id", entries[0].coverArt}, {"size", "128"}}, "fixed-salt", 512 * 1024);
        assert(art.size() > 8 && art[0] == static_cast<char>(0x89) && art[1] == 'P');
        auto audio = api.request("stream", {{"id", "track"}, {"format", "mp3"}, {"maxBitRate", "192"}}, "fixed-salt");
        assert(std::string(audio.begin(), audio.end()) == "fake mp3");
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
