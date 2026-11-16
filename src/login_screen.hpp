#pragma once
#include <filesystem>
#include <future>
#include "login.hpp"
#include "music_surface.hpp"

class LoginScreen : public brls::Activity {
    LoginForm form;
    std::array<brls::Button*, 3> inputs{};
    brls::Button *test = nullptr, *save = nullptr;
    brls::Label* message = nullptr;
    std::shared_ptr<Api> testClient;
    std::future<void> testJob;
    size_t testedVersion = 0;
    bool saving = false;
    std::function<std::string()> makeSalt;
    std::function<void()> saved;
    bool busy() const { return testJob.valid() || saving; }
    void refresh() {
        const char* hints[] = {"app/login_url_hint", "app/login_username", "app/login_password"};
        for (size_t i = 0; i < inputs.size(); ++i) {
            inputs[i]->setText(form.get(i).empty() ? brls::getStr(hints[i]) : i == 2 ? u8"••••••••" : form.get(i));
            inputs[i]->setState(busy() ? brls::ButtonState::DISABLED : brls::ButtonState::ENABLED);
            inputs[i]->setFocusable(!busy()); inputs[i]->setActionAvailable(brls::BUTTON_A, !busy());
        }
        test->setState(busy() ? brls::ButtonState::DISABLED : brls::ButtonState::ENABLED);
        test->setFocusable(!busy()); test->setActionAvailable(brls::BUTTON_A, !busy());
        const bool canSave = !busy() && form.canSave();
        save->setState(canSave ? brls::ButtonState::ENABLED : brls::ButtonState::DISABLED);
        save->setFocusable(canSave); save->setActionAvailable(brls::BUTTON_A, canSave);
    }
    void edit(size_t index) {
        if (busy()) return;
        SwkbdConfig keyboard;
        if (R_FAILED(swkbdCreate(&keyboard, 0))) { showError(brls::getStr("app/login_keyboard_error")); return; }
        if (index == 2) swkbdConfigMakePresetPassword(&keyboard); else swkbdConfigMakePresetDefault(&keyboard);
        const char* titles[] = {"app/login_server", "app/login_username", "app/login_password"};
        swkbdConfigSetHeaderText(&keyboard, brls::getStr(titles[index]).c_str());
        swkbdConfigSetInitialText(&keyboard, form.get(index).c_str());
        swkbdConfigSetStringLenMin(&keyboard, 0);
        swkbdConfigSetStringLenMax(&keyboard, index == 0 ? 512 : index == 1 ? 128 : 256);
        char value[2049]{};
        const auto result = swkbdShow(&keyboard, value, sizeof(value)); swkbdClose(&keyboard);
        if (R_SUCCEEDED(result)) {
            form.set(index, value);
            message->setText(brls::getStr(form.canSave() ? "app/login_test_ok" : "app/login_test_hint"));
            refresh();
        }
    }
    void testConnection() {
        if (busy()) return;
        form.invalidate(); refresh();
        if (!form.complete()) { showError(brls::getStr("app/login_fill_fields")); return; }
        try {
            auto config = form.config();
            testClient = std::make_shared<Api>(config.get());
            const auto client = testClient; const auto salt = makeSalt(); testedVersion = form.version();
            testJob = std::async(std::launch::async, [client, salt] { Api::response(client->request("ping", {}, salt, 64 * 1024)); });
            message->setText(brls::getStr("app/login_testing")); refresh();
        } catch (...) { testClient.reset(); showError(brls::getStr("app/login_invalid_input")); }
    }
    void saveLogin() {
        if (busy() || !form.canSave()) return;
        try {
            std::filesystem::create_directories("sdmc:/switch/navidrome_nx");
            form.save(); saving = true; refresh(); saved();
        } catch (...) { showError(brls::getStr("app/login_save_error")); }
    }
    brls::Label* label(brls::Box* parent, const std::string& text, int size, float height) {
        auto item = new brls::Label(); item->setText(text); item->setFontSize(size); item->setHeight(height); item->setShrink(0);
        parent->addView(item); return item;
    }
public:
    std::function<void()> closed;
    LoginScreen(std::function<std::string()> makeSalt, std::function<void()> saved, std::function<void()> closed)
        : makeSalt(std::move(makeSalt)), saved(std::move(saved)), closed(std::move(closed)) {}
    ~LoginScreen() override {
        if (testClient) testClient->cancel();
        if (testJob.valid()) { testJob.wait(); try { testJob.get(); } catch (...) {} }
        if (closed) closed();
    }
    void showError(const std::string& text) { saving = false; message->setText(text); refresh(); }
    brls::View* createContentView() override {
        auto root = new MusicSurface(); root->setPadding(24, 48, 24, 48);
        root->setJustifyContent(brls::JustifyContent::CENTER); root->setAlignItems(brls::AlignItems::CENTER);
        auto card = new brls::Box(brls::Axis::COLUMN); card->setWidth(820); card->setPadding(28);
        card->setCornerRadius(24); card->setBackgroundColor(nvgRGBA(55, 45, 94, 170));
        card->setBorderThickness(1); card->setBorderColor(nvgRGBA(166, 145, 232, 70));
        auto logo = new brls::Image(); logo->setDimensions(64, 64); logo->setShrink(0); logo->setAlignSelf(brls::AlignSelf::CENTER);
        logo->setImageFromRes("img/navidrome-logo.png"); card->addView(logo);
        label(card, brls::getStr("app/login_title"), 30, 44)->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        label(card, brls::getStr("app/login_subtitle"), 17, 30)->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        const char* names[] = {"app/login_server", "app/login_username", "app/login_password"};
        for (size_t i = 0; i < inputs.size(); ++i) {
            label(card, brls::getStr(names[i]), 16, 24);
            auto button = new brls::Button(); button->setHeight(44); button->setShrink(0); button->setMarginBottom(8);
            button->setJustifyContent(brls::JustifyContent::FLEX_START); button->setPadding(8, 16, 8, 16);
            button->applyXMLAttribute("textHorizontalAlign", "left"); button->applyXMLAttribute("singleLine", "true");
            button->applyXMLAttribute("fontSize", "18");
            button->registerClickAction([this, i](brls::View*) { edit(i); return true; });
            inputs[i] = button; card->addView(button);
        }
        auto actions = new brls::Box(brls::Axis::ROW); actions->setMarginTop(12);
        test = new brls::Button(); test->setText(brls::getStr("app/login_test")); test->setWidth(360); test->setHeight(46); test->setMarginRight(16);
        test->registerClickAction([this](brls::View*) { testConnection(); return true; }); actions->addView(test);
        save = new brls::Button(); save->setText(brls::getStr("app/login_save")); save->setWidth(360); save->setHeight(46); save->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        save->registerClickAction([this](brls::View*) { saveLogin(); return true; }); actions->addView(save); card->addView(actions);
        message = label(card, brls::getStr("app/login_test_hint"), 16, 54); message->setMarginTop(12); message->setTextColor(nvgRGB(195, 200, 225));
        root->addView(card); refresh(); return root;
    }
    void onContentAvailable() override {
        registerAction(brls::getStr("app/back"), brls::BUTTON_B, [this](brls::View*) { message->setText(brls::getStr("app/login_required")); return true; });
    }
    void poll() {
        if (testJob.valid() && testJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try { testJob.get(); form.testSucceeded(testedVersion); message->setText(brls::getStr("app/login_test_ok")); }
            catch (...) { form.invalidate(); message->setText(brls::getStr("app/login_test_failed")); }
            testClient.reset(); refresh();
        }
    }
};
