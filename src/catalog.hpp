#pragma once
#include <functional>
#include "library_logic.hpp"

struct Catalog {
    std::string query;
    std::vector<Entry> entries;
    int order = 0;
    bool loaded = false, grid = false;
};
inline constexpr size_t CatalogLimit = 50000;
inline std::vector<Entry> fetchCatalog(Api& api, EntryKind kind, const std::string& query, const std::function<std::string()>& salt) {
    const bool artists = kind == EntryKind::Artist;
    std::vector<Entry> all;
    for (size_t offset = 0;; offset += 500) {
        auto body = Api::response(api.request("search3", {{"query", query}, {"albumCount", "0"},
            {"artistCount", artists ? "500" : "0"}, {"songCount", artists ? "0" : "500"},
            {artists ? "artistOffset" : "songOffset", std::to_string(offset)}}, salt()));
        auto part = Api::entries(body.get(), "searchResult3", artists ? "artist" : "song");
        // shortcut: bound the local sorting cache; use a server-side index for larger collections.
        if (all.size() + part.size() > CatalogLimit) throw ApiError("response_limit");
        all.insert(all.end(), part.begin(), part.end());
        if (part.size() < 500) break;
    }
    return all;
}
