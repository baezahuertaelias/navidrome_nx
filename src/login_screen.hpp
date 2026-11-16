#pragma once
#include <filesystem>
#include <future>
#include "login.hpp"
#include "music_surface.hpp"

class LoginScreen : public aurore::Activity {
    LoginForm form;
    std::array<MusicButton*, 3> inputs{};
    MusicButton *test = nullptr, *save = nullptr;
    aurore::Label* message = nullptr;
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
            inputs[i]->setText(form.get(i).empty() ? aurore::getStr(hints[i]) : i == 2 ? "••••••••" : form.get(i));
            inputs[i]->setState(busy() ? aurore::ButtonState::DISABLED : aurore::ButtonState::ENABLED);
            inputs[i]->setFocusable(!busy()); inputs[i]->setActionAvailable(aurore::BUTTON_A, !busy());
        }
        test->setState(busy() ? aurore::ButtonState::DISABLED : aurore::ButtonState::ENABLED);
        test->setFocusable(!busy()); test->setActionAvailable(aurore::BUTTON_A, !busy());
        const bool canSave = !busy() && form.canSave();
        save->setState(canSave ? aurore::ButtonState::ENABLED : aurore::ButtonState::DISABLED);
        save->setFocusable(canSave); save->setActionAvailable(aurore::BUTTON_A, canSave);
    }
    void edit(size_t index) {
        if (busy()) return;
        const char* titles[] = {"app/login_server", "app/login_username", "app/login_password"};
        auto entered = [this, index](std::string value) {
            form.set(index, std::move(value));
            message->setText(aurore::getStr(form.canSave() ? "app/login_test_ok" : "app/login_test_hint")); refresh();
        };
        auto ime = aurore::Application::getPlatform()->getImeManager();
        const auto title = aurore::getStr(titles[index]);
        if (index == 2) ime->openForPassword(entered, title, "", 256, form.get(index));
        else ime->openForText(entered, title, "", index == 0 ? 512 : 128, form.get(index));
    }

    void testConnection() {
        if (busy()) return;
        form.invalidate(); refresh();
        if (!form.complete()) { showError(aurore::getStr("app/login_fill_fields")); return; }
        try {
            auto config = form.config();
            testClient = std::make_shared<Api>(config.get());
            const auto client = testClient; const auto salt = makeSalt(); testedVersion = form.version();
            testJob = std::async(std::launch::async, [client, salt] { Api::response(client->request("ping", {}, salt, 64 * 1024)); });
            message->setText(aurore::getStr("app/login_testing")); refresh();
        } catch (...) { testClient.reset(); showError(aurore::getStr("app/login_invalid_input")); }
    }
    void saveLogin() {
        if (busy() || !form.canSave()) return;
        try {
            std::filesystem::create_directories("sdmc:/switch/navidrome_nx");
            form.save(); saving = true; refresh(); saved();
        } catch (...) { showError(aurore::getStr("app/login_save_error")); }
    }
    aurore::Label* label(aurore::Box* parent, const std::string& text, int size, float height) {
        auto item = new aurore::Label(); item->setText(text); item->setFontSize(size); item->setHeight(height); item->setShrink(0);
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
    aurore::View* createContentView() override {
        auto root = new MusicSurface(); root->setPadding(24, 48, 24, 48);
        root->setJustifyContent(aurore::JustifyContent::CENTER); root->setAlignItems(aurore::AlignItems::CENTER);
        auto card = musicCard(aurore::Axis::COLUMN); card->setWidth(820); card->setPadding(28);
        card->setCornerRadius(24); card->setBackgroundColor(nvgRGBA(55, 45, 94, 170));
        card->setBorderThickness(1); card->setBorderColor(nvgRGBA(166, 145, 232, 70));
        auto logo = new aurore::Image(); logo->setDimensions(64, 64); logo->setShrink(0); logo->setAlignSelf(aurore::AlignSelf::CENTER);
        logo->setImageFromRes("img/navidrome-logo.png"); card->addView(logo);
        label(card, aurore::getStr("app/login_title"), 30, 44)->setHorizontalAlign(aurore::HorizontalAlign::CENTER);
        label(card, aurore::getStr("app/login_subtitle"), 17, 30)->setHorizontalAlign(aurore::HorizontalAlign::CENTER);
        const char* names[] = {"app/login_server", "app/login_username", "app/login_password"};
        for (size_t i = 0; i < inputs.size(); ++i) {
            label(card, aurore::getStr(names[i]), 16, 24);
            auto button = new MusicButton(); button->setId("login/field/" + std::to_string(i)); button->setHeight(44); button->setShrink(0); button->setMarginBottom(8);
            button->setJustifyContent(aurore::JustifyContent::FLEX_START); button->setPadding(8, 16, 8, 16);
            button->applyXMLAttribute("textHorizontalAlign", "left"); button->applyXMLAttribute("singleLine", "true");
            button->applyXMLAttribute("fontSize", "18");
            button->registerClickAction([this, i](aurore::View*) { edit(i); return true; });
            inputs[i] = button; card->addView(button);
        }
        auto actions = new aurore::Box(aurore::Axis::ROW); actions->setMarginTop(12);
        test = new MusicButton(); test->setId("login/test"); test->setText(aurore::getStr("app/login_test")); test->setWidth(360); test->setHeight(46); test->setMarginRight(16);
        test->registerClickAction([this](aurore::View*) { testConnection(); return true; }); actions->addView(test);
        save = new MusicButton(); save->setId("login/save"); save->setText(aurore::getStr("app/login_save")); save->setWidth(360); save->setHeight(46); save->setStyle(&aurore::BUTTONSTYLE_PRIMARY);
        save->registerClickAction([this](aurore::View*) { saveLogin(); return true; }); actions->addView(save); card->addView(actions);
        message = label(card, aurore::getStr("app/login_test_hint"), 16, 54); message->setMarginTop(12); message->setTextColor(nvgRGB(195, 200, 225));
        root->addView(card); refresh(); return root;
    }
    void onContentAvailable() override {
        registerAction(aurore::getStr("app/back"), aurore::BUTTON_B, [this](aurore::View*) { message->setText(aurore::getStr("app/login_required")); return true; });
    }
    void poll() {
        if (testJob.valid() && testJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try { testJob.get(); form.testSucceeded(testedVersion); message->setText(aurore::getStr("app/login_test_ok")); }
            catch (...) { form.invalidate(); message->setText(aurore::getStr("app/login_test_failed")); }
            testClient.reset(); refresh();
        }
    }
};
