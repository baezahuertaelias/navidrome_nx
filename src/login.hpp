#pragma once
#include <array>
#include <cstdlib>
#include <optional>
#include "api.hpp"
#include "atomic_file.hpp"

inline constexpr const char* ConfigPath = "sdmc:/switch/navidrome_nx/config.json";
inline void removeSavedLogin(const char* path = ConfigPath) {
    for (const auto& file : {std::string(path) + ".tmp", std::string(path) + ".bak", std::string(path)})
        if (::unlink(file.c_str()) != 0 && errno != ENOENT) throw std::runtime_error("logout_error");
}
inline Json loadLoginConfig(const char* path = ConfigPath) {
    errno = 0;
    auto file = std::fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT) return jsonOwner(nullptr);
        throw std::runtime_error("login_config_read");
    }
    auto config = jsonOwner(json_loadf(file, JSON_REJECT_DUPLICATES, nullptr));
    std::fclose(file);
    if (!config) throw std::runtime_error("login_config_invalid");
    Api validate(config.get());
    return config;
}
class LoginForm {
    std::array<std::string, 3> fields;
    size_t revision = 0;
    std::optional<size_t> verified;
public:
    const std::string& get(size_t index) const { return fields.at(index); }
    void set(size_t index, std::string value) {
        if (fields.at(index) == value) return;
        fields[index] = std::move(value); ++revision; verified.reset();
    }
    size_t version() const { return revision; }
    void invalidate() { verified.reset(); }
    void testSucceeded(size_t testedVersion) { if (testedVersion == revision) verified = revision; }
    bool complete() const { return std::all_of(fields.begin(), fields.end(), [](const auto& value) { return !value.empty(); }); }
    bool canSave() const { return complete() && verified && *verified == revision; }
    Json config() const {
        return jsonOwner(json_pack("{s:s,s:s,s:s}", "server", fields[0].c_str(), "username", fields[1].c_str(), "password", fields[2].c_str()));
    }
    void save(const char* path = ConfigPath) const {
        if (!canSave()) throw std::runtime_error("login_test_required");
        auto root = config();
        if (!root) throw std::runtime_error("login_save_error");
        Api validate(root.get());
        char* encoded = json_dumps(root.get(), JSON_INDENT(2));
        if (!encoded) throw std::runtime_error("login_save_error");
        const std::string payload(encoded); std::free(encoded);
        writeAtomicFile(path, payload);
    }
};
