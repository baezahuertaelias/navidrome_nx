#pragma once
#include "api.hpp"
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>

class SongCache {
    std::filesystem::path root;
    static constexpr size_t ByteLimit = 64 * 1024 * 1024;
    static bool songFile(const std::string& name) {
        return name.size() == 36 && name.substr(32) == ".mp3" && name.find_first_not_of("0123456789abcdef") == 32;
    }
    std::filesystem::path path(const Api& api, const std::string& id) const {
        const auto input = std::to_string(api.serverUrl().size()) + ":" + api.serverUrl() + std::to_string(api.userName().size()) + ":" + api.userName() + id;
        unsigned char hash[16];
        if (mbedtls_md5_ret(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash)) throw std::runtime_error("cache_key");
        std::string name;
        for (auto value : hash) { name += "0123456789abcdef"[value >> 4]; name += "0123456789abcdef"[value & 15]; }
        return root / (name + ".mp3");
    }
    void trim(size_t keep) const {
        std::vector<std::filesystem::directory_entry> files;
        for (const auto& file : std::filesystem::directory_iterator(root)) {
            const auto name = file.path().filename().string();
            if (file.is_regular_file() && songFile(name)) files.push_back(file);
            else if (file.is_regular_file() && name.size() == 40 && name.substr(36) == ".tmp" && songFile(name.substr(0, 36))) std::filesystem::remove(file.path());
        }
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.last_write_time() < b.last_write_time(); });
        for (size_t i = 0; i + keep < files.size(); ++i) std::filesystem::remove(files[i].path());
    }
public:
    static constexpr size_t MaxSongs = 15;
    explicit SongCache(std::filesystem::path root = "sdmc:/switch/navidrome_nx/song-cache") : root(std::move(root)) {}
    void erase(const Api& api, const std::string& id) const { std::filesystem::remove(path(api, id)); }
    std::vector<char> load(const Api& api, const std::string& id) const {
        const auto file = path(api, id);
        if (!std::filesystem::exists(file)) return {};
        const auto size = std::filesystem::file_size(file);
        if (!size || size > ByteLimit) { std::filesystem::remove(file); return {}; }
        std::ifstream input(file, std::ios::binary);
        std::vector<char> bytes(size);
        if (!input.read(bytes.data(), size)) { std::filesystem::remove(file); return {}; }
        ::utime(file.c_str(), nullptr);
        return bytes;
    }
    void save(const Api& api, const std::string& id, const std::vector<char>& bytes) const {
        if (bytes.empty() || bytes.size() > ByteLimit) throw std::runtime_error("cache_size");
        std::filesystem::create_directories(root);
        const auto file = path(api, id);
        if (std::filesystem::exists(file)) { ::utime(file.c_str(), nullptr); trim(MaxSongs); return; }
        trim(MaxSongs - 1);
        const auto temp = file.string() + ".tmp";
        const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) throw std::runtime_error("cache_write");
        bool ok = true;
        for (size_t offset = 0; offset < bytes.size();) {
            const auto count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { ok = false; break; }
            offset += count;
        }
        if (::fsync(fd)) ok = false;
        if (::close(fd)) ok = false;
        if (!ok || ::rename(temp.c_str(), file.c_str())) {
            ::unlink(temp.c_str()); throw std::runtime_error("cache_write");
        }
    }
};
