// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "api.hpp"
#include "atomic_file.hpp"
#include "library.hpp"
#include <dirent.h>
#include <set>
#include <sys/stat.h>

namespace overlay {
inline constexpr const char* CacheRoot = "sdmc:/switch/navidrome_nx/overlay-cache";
inline constexpr size_t TrackLimit = 24 * 1024 * 1024, CacheLimit = 256 * 1024 * 1024;
inline std::string cacheKey(const Api& api, const std::string& id) {
    const auto text = std::to_string(api.serverUrl().size()) + ":" + api.serverUrl() + std::to_string(api.userName().size()) + ":" + api.userName() + id;
    unsigned char digest[16];
    if (mbedtls_md5_ret(reinterpret_cast<const unsigned char*>(text.data()), text.size(), digest)) throw std::runtime_error("Cache key failed");
    std::string key;
    for (auto byte : digest) { key += "0123456789abcdef"[byte >> 4]; key += "0123456789abcdef"[byte & 15]; }
    return key;
}
inline bool cacheName(const std::string& name) {
    return name.size() == 36 && name.substr(32) == ".mp3" && name.find_first_not_of("0123456789abcdef") == 32;
}
inline void ensureDirectory(const std::string& root) {
    if (::mkdir(root.c_str(), 0700) && errno != EEXIST) throw std::runtime_error("Cannot create overlay cache");
    struct stat info{};
    if (::stat(root.c_str(), &info) || !S_ISDIR(info.st_mode)) throw std::runtime_error("Invalid cache directory");
}
inline void reserveCache(const std::string& root, const std::set<std::string>& protectedFiles, size_t reserve = TrackLimit) {
    struct File { std::string path; size_t size; time_t modified; };
    auto closeDirectory = [](DIR* directory) { closedir(directory); };
    std::unique_ptr<DIR, decltype(closeDirectory)> dir(opendir(root.c_str()), closeDirectory);
    if (!dir) throw std::runtime_error("Cannot read overlay cache");
    std::vector<File> files; size_t total = 0;
    while (auto entry = readdir(dir.get())) {
        const std::string name = entry->d_name;
        if (name.size() == 41 && name.substr(36) == ".part" && cacheName(name.substr(0, 36))) {
            ::unlink((root + "/" + name).c_str()); continue;
        }
        if (!cacheName(name)) continue;
        const auto path = root + "/" + entry->d_name;
        struct stat info{};
        if (::lstat(path.c_str(), &info) || !S_ISREG(info.st_mode)) continue;
        total += static_cast<size_t>(info.st_size);
        if (!protectedFiles.count(path)) files.push_back({path, static_cast<size_t>(info.st_size), info.st_mtime});
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.modified < b.modified; });
    for (const auto& file : files) {
        if (reserve <= CacheLimit && total <= CacheLimit - reserve) break;
        if (::unlink(file.path.c_str())) throw std::runtime_error("Cannot free overlay cache");
        ::unlink((file.path + ".json").c_str()); total -= file.size;
    }
    if (reserve > CacheLimit || total > CacheLimit - reserve) throw std::runtime_error("Cache full: clear the current queue before downloading more");
}
inline void saveMetadata(const std::string& path, const Entry& entry) {
    auto root = jsonOwner(json_pack("{s:s,s:s,s:s,s:s,s:I}", "id", entry.id.c_str(), "title", entry.title.c_str(), "artist", entry.artist.c_str(), "coverArt", entry.coverArt.c_str(), "duration", static_cast<json_int_t>(entry.duration)));
    if (!root) throw std::runtime_error("Metadata allocation failed");
    std::unique_ptr<char, decltype(&std::free)> text(json_dumps(root.get(), JSON_COMPACT), std::free);
    if (!text) throw std::runtime_error("Metadata allocation failed");
    writeAtomicFile((path + ".json").c_str(), text.get());
}
inline Entry readMetadata(const std::string& path, const std::string& root = CacheRoot) {
    if (path.rfind(root + "/", 0) != 0 || !cacheName(path.substr(root.size() + 1))) return {};
    auto file = std::unique_ptr<FILE, decltype(&std::fclose)>(std::fopen((path + ".json").c_str(), "rb"), std::fclose);
    if (!file) return {};
    char bytes[8193]; const auto count = std::fread(bytes, 1, sizeof(bytes), file.get());
    if (count == sizeof(bytes)) return {};
    auto object = jsonOwner(json_loadb(bytes, count, JSON_REJECT_DUPLICATES, nullptr));
    if (!object) return {};
    auto get = [&](const char* key) { const char* value = json_string_value(json_object_get(object.get(), key)); return std::string(value ? value : ""); };
    Entry result; result.id = get("id"); result.title = get("title"); result.artist = get("artist"); result.coverArt = get("coverArt"); result.duration = json_integer_value(json_object_get(object.get(), "duration")); return result;
}
inline std::string cacheTrack(const Api& api, const Entry& entry, std::set<std::string>& protectedFiles, const std::string& root = CacheRoot) {
    ensureDirectory(root);
    const auto path = root + "/" + cacheKey(api, entry.id) + ".mp3";
    struct stat info{};
    if (::stat(path.c_str(), &info) || !S_ISREG(info.st_mode) || info.st_size <= 0 || static_cast<size_t>(info.st_size) > TrackLimit) {
        reserveCache(root, protectedFiles);
        const auto temp = path + ".part";
        auto file = std::unique_ptr<FILE, decltype(&std::fclose)>(std::fopen(temp.c_str(), "wb"), std::fclose);
        if (!file) throw std::runtime_error("Cannot write overlay cache");
        try {
            api.request("stream", {{"id", entry.id}, {"format", "mp3"}, {"maxBitRate", "192"}}, salt(), TrackLimit, file.get());
            if (std::fflush(file.get()) || ::fsync(fileno(file.get()))) throw std::runtime_error("Cannot save downloaded track");
            if (::fstat(fileno(file.get()), &info) || info.st_size <= 0) throw std::runtime_error("Empty track download");
            file.reset();
            if (::rename(temp.c_str(), path.c_str())) throw std::runtime_error("Cannot finish downloaded track");
        } catch (...) { file.reset(); ::unlink(temp.c_str()); throw; }
    }
    saveMetadata(path, entry); protectedFiles.insert(path); return path;
}
}
