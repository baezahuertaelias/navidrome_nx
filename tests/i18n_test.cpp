#include <borealis/core/application.hpp>
#include <borealis/core/i18n.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

static std::string systemLocale;
// Application headers initialize colors; this test never creates a GPU renderer.
NVGcolor nvgRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    NVGcolor color;
    color.r = r / 255.0f; color.g = g / 255.0f;
    color.b = b / 255.0f; color.a = a / 255.0f;
    return color;
}
namespace brls {
std::string Application::getLocale() { return systemLocale; }
}

int main() {
    const std::filesystem::path root = "/tmp/navidrome-i18n-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "i18n");
    std::filesystem::copy("resources/i18n", root / "i18n", std::filesystem::copy_options::recursive);
    systemLocale = "es-419";
    brls::loadTranslations();
    assert(brls::getStr("app/albums") == "Álbumes");
    assert(brls::getStr("app/queue_position", 2, 50) == "Pista 2/50");
    assert(brls::getStr("brls/hints/back") == "Volver");
    assert(brls::getStr("app/error/subsonic", "40").find("credenciales") != std::string::npos);
    systemLocale = "es";
    brls::loadTranslations();
    assert(brls::getStr("app/lyrics") == "Letra");
    assert(brls::getStr("app/opening_album", "{album}") == "Abriendo {album}...");
    std::filesystem::create_directories(root / "i18n/es-XX");
    std::ofstream(root / "i18n/es-XX/app.json") << R"({"albums":"Regional albums","error":{"network":"Regional network: {0}"}})";
    systemLocale = "es-XX";
    brls::loadTranslations();
    assert(brls::getStr("app/albums") == "Regional albums");
    assert(brls::getStr("app/lyrics") == "Letra");
    assert(brls::getStr("app/error/network", "detail") == "Regional network: detail");
    assert(brls::getStr("app/error/http_status", "503") == "Error HTTP 503");
    for (const auto* locale : {"en-US", "en-GB", "ja", "pt-BR", "unknown"}) {
        systemLocale = locale;
        brls::loadTranslations();
        assert(brls::getStr("app/albums") == "Albums");
        assert(brls::getStr("app/queue_position", 1, 2) == "Track 1 of 2");
    }
    systemLocale = "fr-CA";
    brls::loadTranslations();
    assert(brls::getStr("brls/hints/back") == "Retour");
    assert(brls::getStr("app/albums") == "Albums");
    std::cout << "System-locale and fallback checks passed\n";
}
