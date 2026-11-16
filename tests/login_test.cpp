#include "login.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>

template<class F> void rejects(F fn) {
    bool rejected = false; try { fn(); } catch (...) { rejected = true; } assert(rejected);
}
int main() {
    const auto root = std::filesystem::path("/tmp/navidrome-logout-test");
    std::filesystem::create_directory(root);
    const auto config = (root / "config.json").string();
    const auto settings = root / "settings.json";
    std::ofstream(settings) << "preferences";
    assert(!loadLoginConfig(config.c_str()));
    LoginForm form; assert(!form.complete() && !form.canSave());
    rejects([&] { form.save(config.c_str()); }); assert(!std::filesystem::exists(config));
    form.set(0, "http://localhost/music"); form.set(1, "a user+&"); form.set(2, "s3cret");
    assert(form.complete() && !form.canSave());
    const auto stale = form.version(); form.set(2, "s3cret2"); form.testSucceeded(stale);
    assert(!form.canSave());
    form.testSucceeded(form.version()); assert(form.canSave());
    form.save(config.c_str());
    auto saved = loadLoginConfig(config.c_str());
    assert(field(saved.get(), "server") == "http://localhost/music" && field(saved.get(), "password") == "s3cret2");
    form.set(2, "s3cret2"); assert(form.canSave());
    form.set(1, "another user"); assert(!form.canSave());
    rejects([&] { form.save(config.c_str()); });
    saved = loadLoginConfig(config.c_str()); assert(field(saved.get(), "username") == "a user+&");
    form.testSucceeded(form.version()); form.save(config.c_str());
    saved = loadLoginConfig(config.c_str()); assert(field(saved.get(), "username") == "another user");
    assert(!std::filesystem::exists(config + ".tmp") && !std::filesystem::exists(config + ".bak"));
    form.set(0, "ftp://invalid"); form.testSucceeded(form.version());
    rejects([&] { form.save(config.c_str()); }); assert(loadLoginConfig(config.c_str()));
    form.set(0, "http://localhost/music"); form.testSucceeded(form.version());
    assert(std::rename(config.c_str(), (config + ".bak").c_str()) == 0);
    assert(!loadLoginConfig(config.c_str())); // No config.json always requires login, even with an old backup.
    form.save(config.c_str()); assert(!std::filesystem::exists(config + ".bak"));
    rejects([&] { form.save((root / "missing" / "config.json").c_str()); });
    assert(loadLoginConfig(config.c_str()));
    rejects([&] { form.save(root.c_str()); }); assert(std::filesystem::exists(settings));
    for (const char* bad : {"not json", "{}", "{\"server\":\"x\",\"server\":\"y\"}"}) {
        std::ofstream(config) << bad;
        rejects([&] { loadLoginConfig(config.c_str()); });
    }
    form.save(config.c_str());
    form.invalidate(); assert(!form.canSave());
    rejects([&] { form.save(config.c_str()); });
    std::ofstream(config + ".tmp") << "temporary login";
    std::ofstream(config + ".bak") << "backup login";
    removeSavedLogin(config.c_str());
    assert(!loadLoginConfig(config.c_str()) && std::filesystem::exists(settings));
    assert(!std::filesystem::exists(config + ".tmp") && !std::filesystem::exists(config + ".bak"));
    removeSavedLogin(config.c_str());
    rejects([&] { removeSavedLogin(root.c_str()); });
    assert(std::filesystem::exists(settings));
    std::filesystem::remove(settings); std::filesystem::remove(root);
}
