#pragma once
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

inline void writeAtomicFile(const std::string& path, const std::string& payload) {
    const std::string temporary = path + ".tmp";
    const int file = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file < 0) throw std::runtime_error("file_write");
    bool ok = true;
    for (size_t sent = 0; sent < payload.size();) {
        const auto count = ::write(file, payload.data() + sent, payload.size() - sent);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        sent += static_cast<size_t>(count);
    }
    if (::fsync(file) != 0) ok = false;
    if (::close(file) != 0) ok = false;
    const std::string backup = std::string(path) + ".bak";
    struct stat existing{};
    const bool hadOriginal = ::stat(path.c_str(), &existing) == 0;
    if ((!hadOriginal && errno != ENOENT) || (hadOriginal && !S_ISREG(existing.st_mode))) {
        std::remove(temporary.c_str()); throw std::runtime_error("file_write");
    }
    if (ok && hadOriginal) {
        // libnx rename cannot replace an existing destination on the SD card.
        if (std::remove(backup.c_str()) != 0 && errno != ENOENT) ok = false;
        if (ok && std::rename(path.c_str(), backup.c_str()) != 0) ok = false;
    }
    if (!ok || std::rename(temporary.c_str(), path.c_str()) != 0) {
        if (hadOriginal && ::access(path.c_str(), F_OK) != 0) std::rename(backup.c_str(), path.c_str());
        std::remove(temporary.c_str());
        throw std::runtime_error("file_write");
    }
    std::remove(backup.c_str());
}
