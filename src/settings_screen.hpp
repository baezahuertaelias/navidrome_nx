#pragma once
#include <filesystem>
#include "music_surface.hpp"
#include "clock_text.hpp"
#include "pairing_qr.hpp"

class SettingsScreen : public brls::Activity {
    brls::Label *message = nullptr, *remoteAddress = nullptr, *header = nullptr;
    std::vector<std::pair<brls::Button*, std::function<std::string()>>> values;
    std::function<void()> changed;
    std::function<void()> logout;
    std::string accountServer, accountUser;
    std::function<std::string()> remoteInfo;
    std::string lastClock, lastRemote;
    std::function<std::string()> remotePairing;
    brls::Box* qrPanel = nullptr;
    PairingQr* qr = nullptr;
    brls::ScrollingFrame* settingsScroll = nullptr;
    std::array<brls::Box*, 4> pages{};
    std::array<brls::Button*, 4> tabs{};
    int activeTab = 0;
    bool lastQrVisible = false;
    brls::Box *bubbleRow = nullptr, *opacityRow = nullptr, *styleRow = nullptr, *speedRow = nullptr;
    brls::Label* appearanceHint = nullptr;
    void updateAppearance() {
        const bool colored = preferences.background != 4;
        for (auto row : {opacityRow, styleRow, speedRow}) row->setVisibility(colored ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        bubbleRow->setVisibility(colored && preferences.backgroundStyle == 0 ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
        appearanceHint->setText(brls::getStr(!colored ? "app/settings_amoled_hint" : preferences.background == 5 ? "app/settings_rainbow_hint" : "app/settings_appearance_hint"));
    }
    void selectTab(int index) {
        activeTab = index;
        for (size_t i = 0; i < pages.size(); ++i) {
            pages[i]->setVisibility(static_cast<int>(i) == index ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
            tabs[i]->setStyle(static_cast<int>(i) == index ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
            assert(static_cast<int>(i) == index || pages[i]->getDefaultFocus() == nullptr);
        }
        if (auto first = pages[index]->getDefaultFocus()) first->setCustomNavigationRoute(brls::FocusDirection::UP, tabs[index]);
        settingsScroll->willAppear(true);
        if (message) { message->setText(brls::getStr("app/settings_adjust_hint")); message->setTextColor(nvgRGB(188, 192, 215)); }
    }
    brls::Label* section(brls::Box* parent, const char* title, const char* description) {
        auto label = new brls::Label(); label->setText(brls::getStr(title)); label->setFontSize(24); label->setHeight(36); label->setShrink(0); parent->addView(label);
        auto detail = new brls::Label(); detail->setText(brls::getStr(description)); detail->setFontSize(16); detail->setHeight(40); detail->setShrink(0); detail->setTextColor(nvgRGB(188, 192, 215)); parent->addView(detail); return detail;
    }
    brls::Box* option(brls::Box* parent, const std::string& name, std::function<std::string()> value,
        std::function<void(int)> change) {
        auto row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(8); row->setPadding(6, 10, 6, 10); row->setCornerRadius(12); row->setBackgroundColor(nvgRGBA(91, 85, 140, 45));
        auto label = new brls::Label(); label->setText(name); label->setFontSize(20);
        label->setWidth(0); label->setGrow(1); label->setHeight(42);
        row->addView(label);
        auto button = new brls::Button(); button->setText(u8"‹ " + value() + u8" ›"); button->setWidth(300); button->setHeight(42); button->setShrink(0); button->applyXMLAttribute("fontSize", "18");
        auto adjust = [this, change](int direction) {
            const auto previous = preferences;
            change(direction);
            try {
                std::filesystem::create_directories("sdmc:/switch/navidrome_nx");
                saveSettings(preferences);
                changed();
                message->setText(brls::getStr("app/settings_saved")); message->setTextColor(nvgRGB(147, 224, 196));
            } catch (...) {
                preferences = previous;
                message->setText(brls::getStr("app/settings_save_error")); message->setTextColor(nvgRGB(255, 153, 164));
            }
            for (auto& item : values) item.first->setText(u8"‹ " + item.second() + u8" ›");
            updateAppearance();
        };
        button->registerClickAction([adjust](brls::View*) { adjust(1); return true; });
        button->registerAction("", brls::BUTTON_LEFT, [adjust](brls::View*) { adjust(-1); return true; }, true);
        button->registerAction("", brls::BUTTON_RIGHT, [adjust](brls::View*) { adjust(1); return true; }, true);
        values.push_back({button, value}); row->addView(button); parent->addView(row); return row;
    }
    std::string toggle(bool value) { return brls::getStr(value ? "app/on" : "app/off"); }
public:
    std::function<void()> closed;
    SettingsScreen(std::function<void()> changed, std::function<std::string()> remoteInfo, std::function<std::string()> remotePairing,
        std::function<void()> closed, std::function<void()> logout, std::string accountServer, std::string accountUser) : changed(std::move(changed)), logout(std::move(logout)), accountServer(std::move(accountServer)), accountUser(std::move(accountUser)), remoteInfo(std::move(remoteInfo)), remotePairing(std::move(remotePairing)), closed(std::move(closed)) {}
    ~SettingsScreen() override { if (closed) closed(); }
    brls::View* createContentView() override {
        auto root = new MusicSurface(); root->setPadding(24, 48, 20, 48);
        auto headerRow = new brls::Box(brls::Axis::ROW); headerRow->setHeight(56); headerRow->setShrink(0);
        header = new brls::Label(); header->setText(brls::getStr("app/settings")); header->setFontSize(30); header->setHeight(48); header->setWidth(0); header->setGrow(1); headerRow->addView(header);
        auto back = new brls::Button(); back->setText(brls::getStr("app/back")); back->setWidth(140); back->setHeight(42); back->setShrink(0);
        back->registerClickAction([](brls::View*) { brls::Application::popActivity(); return true; }); headerRow->addView(back); root->addView(headerRow);
        const char* tabKeys[] = {"app/settings_appearance", "app/settings_layout", "app/settings_remote", "app/settings_account"};
        auto tabRow = new brls::Box(brls::Axis::ROW); tabRow->setHeight(44); tabRow->setShrink(0); tabRow->setMarginBottom(16);
        for (size_t i = 0; i < tabs.size(); ++i) {
            auto button = new brls::Button(); button->setText(brls::getStr(tabKeys[i])); button->setHeight(44); button->setWidth(0); button->setGrow(1); button->applyXMLAttribute("fontSize", "18");
            if (i + 1 < tabs.size()) button->setMarginRight(12);
            button->registerClickAction([this, i](brls::View*) { selectTab(static_cast<int>(i)); return true; }); tabs[i] = button; tabRow->addView(button);
        }
        root->addView(tabRow);
        auto body = new brls::Box(brls::Axis::ROW); body->setHeight(0); body->setGrow(1); root->addView(body);
        settingsScroll = new brls::ScrollingFrame(); settingsScroll->setWidth(0); settingsScroll->setGrow(1); body->addView(settingsScroll);
        auto options = new brls::Box(brls::Axis::COLUMN); options->setWidth(1184); options->setPadding(12);
        for (auto& page : pages) { page = new brls::Box(brls::Axis::COLUMN); options->addView(page); }
        appearanceHint = section(pages[0], "app/settings_appearance", "app/settings_appearance_hint");
        option(pages[0], brls::getStr("app/background"), [] {
            const char* colors[] = {"app/color_purple", "app/color_blue", "app/color_rose", "app/color_emerald", "app/color_amoled", "app/color_rainbow"};
            return brls::getStr(colors[preferences.background]);
        }, [](int direction) { preferences.background = adjustSetting(preferences.background, 0, 5, direction); });
        opacityRow = option(pages[0], brls::getStr("app/background_opacity"), [] { return std::to_string(preferences.backgroundOpacity) + "%"; },
            [](int direction) { preferences.backgroundOpacity = std::clamp(preferences.backgroundOpacity + direction * 10, 0, 100); });
        styleRow = option(pages[0], brls::getStr("app/background_style"), [] { return brls::getStr(preferences.backgroundStyle == 0 ? "app/style_bubbles" : "app/style_xmb"); },
            [](int direction) { preferences.backgroundStyle = adjustSetting(preferences.backgroundStyle, 0, 1, direction); });
        bubbleRow = option(pages[0], brls::getStr("app/bubble_count"), [] { return std::to_string(preferences.bubbleCount); },
            [](int direction) { preferences.bubbleCount = adjustSetting(preferences.bubbleCount, 0, MaxBubbleCount, direction); });
        speedRow = option(pages[0], brls::getStr("app/animation_speed"), [] {
            const char* speeds[] = {"app/off", "app/slow", "app/normal", "app/fast"}; return brls::getStr(speeds[preferences.bubbleSpeed]);
        }, [](int direction) { preferences.bubbleSpeed = adjustSetting(preferences.bubbleSpeed, 0, 3, direction); });
        section(pages[1], "app/settings_layout", "app/settings_layout_hint");
        option(pages[1], brls::getStr("app/show_clock"), [this] { return toggle(preferences.showClock); }, [](int) { preferences.showClock = !preferences.showClock; });
        option(pages[1], brls::getStr("app/items_per_row"), [] { return std::to_string(preferences.columns); }, [](int direction) { preferences.columns = adjustSetting(preferences.columns, 4, 6, direction); });
        option(pages[1], brls::getStr("app/collapse_menu"), [this] { return toggle(preferences.sidebarCollapsed); }, [](int) { preferences.sidebarCollapsed = !preferences.sidebarCollapsed; });
        option(pages[1], brls::getStr("app/cache_songs"), [this] { return toggle(preferences.cacheSongs); }, [](int) { preferences.cacheSongs = !preferences.cacheSongs; });
        section(pages[2], "app/settings_remote", "app/settings_remote_hint");
        option(pages[2], brls::getStr("app/web_remote"), [this] { return toggle(preferences.remoteEnabled); }, [](int) { preferences.remoteEnabled = !preferences.remoteEnabled; });
        remoteAddress = new brls::Label(); remoteAddress->setFontSize(18); remoteAddress->setHeight(180); remoteAddress->setShrink(0); pages[2]->addView(remoteAddress);
        section(pages[3], "app/settings_account", "app/settings_account_hint");
        const std::pair<const char*, std::string> accountFields[] = {{"app/login_server", accountServer}, {"app/login_username", accountUser}};
        for (const auto& field : accountFields) {
            auto name = new brls::Label(); name->setText(brls::getStr(field.first)); name->setFontSize(16); name->setHeight(26); name->setShrink(0); name->setTextColor(nvgRGB(188, 192, 215)); pages[3]->addView(name);
            auto value = new brls::Label(); value->setText(field.second.empty() ? "—" : field.second); value->setFontSize(20); value->setSingleLine(false); value->setWidthPercentage(100); value->setShrink(0); value->setMarginBottom(16); pages[3]->addView(value);
        }
        auto logoutButton = new brls::Button(); logoutButton->setText(brls::getStr("app/logout")); logoutButton->setWidth(300); logoutButton->setHeight(46); logoutButton->setMarginTop(12);
        logoutButton->registerClickAction([this](brls::View*) { try { logout(); } catch (...) { message->setText(brls::getStr("app/logout_error")); } return true; }); pages[3]->addView(logoutButton);
        settingsScroll->setContentView(options);
        qrPanel = new brls::Box(brls::Axis::COLUMN); qrPanel->setWidth(280); qrPanel->setShrink(0); qrPanel->setMarginLeft(24); qrPanel->setVisibility(brls::Visibility::GONE);
        auto scan = new brls::Label(); scan->setText(brls::getStr("app/scan_remote")); scan->setFontSize(20); scan->setHeight(70); qrPanel->addView(scan);
        qr = new PairingQr(); qrPanel->addView(qr); body->addView(qrPanel);
        message = new brls::Label(); message->setFontSize(16); message->setHeight(36); message->setShrink(0); message->setText(brls::getStr("app/settings_adjust_hint")); root->addView(message);
        updateAppearance();
        selectTab(0); return root;
    }
    void onContentAvailable() override {
        registerAction(brls::getStr("app/back"), brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });
    }
    void poll() {
        const auto clock = preferences.showClock ? clockText() : "";
        if (clock != lastClock) { lastClock = clock; header->setText((clock.empty() ? "" : clock + "   ") + brls::getStr("app/settings")); }
        const auto pairing = remotePairing();
        qr->update(pairing);
        const bool visible = activeTab == 2 && !pairing.empty();
        if (visible != lastQrVisible) { lastQrVisible = visible; qrPanel->setVisibility(visible ? brls::Visibility::VISIBLE : brls::Visibility::GONE); }
        const auto address = remoteInfo();
        if (address != lastRemote) { lastRemote = address; remoteAddress->setText(address); }
    }
};
