#pragma once
#include <functional>
#include "api.hpp"
#include "music_surface.hpp"
#include "playback_clock.hpp"
#include "clock_text.hpp"
#include "lyric_emphasis.hpp"
#include "audio_details.hpp"
#include "lyrics_navigation.hpp"

class LyricsPane : public brls::View, public LyricsNavigation {
public:
    SongLyrics lyrics;
    int textSize = 26;
    std::string message;
    LyricEmphasis emphasis;

    LyricsPane() {
        setFocusable(false);
        setHideHighlightBackground(true);
        setHideHighlight(true);
        setHighlightCornerRadius(14);
        registerAction("", brls::BUTTON_UP, [this](brls::View*) { move(-1); return true; }, true);
        registerAction("", brls::BUTTON_DOWN, [this](brls::View*) { move(1); return true; }, true);
    }
    bool onTouchScroll(float, float dy) override { return scroll(dy, lyrics.lines.size()); }
    void move(int direction) { LyricsNavigation::move(direction, lyrics.lines.size()); }
    void update(long long elapsedMs) {
        updateActive(lyrics.activeLine(elapsedMs));
        emphasis.update(active, std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style, brls::FrameContext*) override {
        nvgSave(vg);
        nvgIntersectScissor(vg, x, y, width, height);
        nvgFontFaceId(vg, brls::Application::getFont(brls::FONT_REGULAR));
        nvgFontSize(vg, textSize);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgTextLineHeight(vg, 1.3f);
        if (lyrics.empty()) {
            nvgFillColor(vg, nvgRGB(186, 192, 219));
            nvgTextBox(vg, x + 20, y + 32, width - 48, message.c_str(), nullptr);
        } else {
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            const int regular = brls::Application::getFont(brls::FONT_REGULAR), bold = brls::Application::getFont("lyrics-bold");
            const float maxSize = textSize * 1.08f, textWidth = width - 56;
            float top = y + 12;
            for (size_t i = static_cast<size_t>(std::max(0, selected - 2)); i < lyrics.lines.size() && top < y + height; ++i) {
                const auto& text = lyrics.lines[i].text;
                const float weight = emphasis.amount(static_cast<int>(i), seconds);
                nvgFontFaceId(vg, bold); nvgFontSize(vg, maxSize);
                float bounds[4], lineHeight;
                nvgTextMetrics(vg, nullptr, nullptr, &lineHeight);
                nvgTextBoxBounds(vg, 0, 0, textWidth, text.empty() ? " " : text.c_str(), nullptr, bounds);
                const float rowHeight = std::max(64.0f, bounds[3] - bounds[1] + 28);
                if (weight > 0 || (!follow && static_cast<int>(i) == selected)) {
                    nvgBeginPath(vg); nvgRoundedRect(vg, x + 8, top, width - 24, rowHeight - 6, 14);
                    nvgFillColor(vg, nvgRGBA(116, 89, 193, static_cast<unsigned char>(20 + weight * 30))); nvgFill(vg);
                }
                NVGtextRow rows[2]; const char* next = text.c_str(); int count;
                float textY = top + 12 + (maxSize - textSize * (1 + 0.08f * weight)) / 2;
                while (*next) {
                    // Wrap at the largest bold size so growing text keeps its line breaks.
                    nvgFontFaceId(vg, bold); nvgFontSize(vg, maxSize);
                    count = nvgTextBreakLines(vg, next, nullptr, textWidth, rows, 2);
                    if (!count) break;
                    for (int row = 0; row < count; ++row) {
                        nvgFontSize(vg, textSize * (1 + 0.08f * weight));
                        nvgFontFaceId(vg, regular);
                        nvgFillColor(vg, nvgRGBA(185, 187, 213, static_cast<unsigned char>(255 * (1 - weight))));
                        nvgText(vg, x + (width - 8) / 2, textY, rows[row].start, rows[row].end);
                        nvgFontFaceId(vg, bold);
                        nvgFillColor(vg, nvgRGBA(255, 255, 255, static_cast<unsigned char>(255 * weight)));
                        nvgText(vg, x + (width - 8) / 2, textY, rows[row].start, rows[row].end);
                        textY += lineHeight * 1.3f;
                    }
                    next = rows[count - 1].next;
                }
                top += rowHeight;
            }
            const float handle = std::max(24.0f, height / lyrics.lines.size());
            const float position = lyrics.lines.size() > 1 ? selected * (height - handle) / (lyrics.lines.size() - 1) : 0;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, x + width - 6, y + position, 3, handle, 1.5f);
            nvgFillColor(vg, nvgRGBA(150, 204, 255, 140));
            nvgFill(vg);
        }
        nvgRestore(vg);
    }
};

class LyricsScreen : public brls::Activity {
    brls::Image* artwork = nullptr;
    brls::Label *title = nullptr, *artist = nullptr, *time = nullptr, *state = nullptr, *subtitle = nullptr, *brand = nullptr;
    brls::Rectangle* progress = nullptr;
    brls::Label* sourceInfo = nullptr;
    brls::Button *pauseButton = nullptr, *followButton = nullptr, *manualButton = nullptr;
    brls::Label* manualState = nullptr;
    LyricsPane* pane = nullptr;
    std::string trackId, artId;
    long long lastSecond = -1;
    bool lastPaused = false, lastPlaying = false;
    bool lastFollow = false, lastSynced = false, lastManual = false;
    std::string lastClock;
    std::function<void()> pause, previous, next;

    brls::Label* label(brls::Box* parent, const std::string& text, int size, float height) {
        auto view = new brls::Label();
        view->setText(text); view->setFontSize(size); view->setHeight(height);
        view->setSingleLine(true); view->setShrink(0);
        parent->addView(view); return view;
    }
    brls::Button* button(brls::Box* parent, const std::string& icon, const std::string& caption, std::function<void()> callback) {
        auto view = new brls::Button();
        view->applyXMLAttribute("glyphCentered", "true");
        view->applyXMLAttribute("font", "music"); view->applyXMLAttribute("fontSize", "26");
        view->setText(icon); view->setHeight(44); view->setWidth(64); view->setShrink(0);
        view->setPadding(8, 14, 8, 14); view->setMarginRight(10); view->setCornerRadius(12);
        view->registerClickAction([callback](brls::View*) { callback(); return true; });
        view->updateActionHint(brls::BUTTON_A, caption);
        parent->addView(view); return view;
    }
    void setManual(bool enabled) {
        pane->setManual(enabled, pane->lyrics.synced);
        pane->setFocusable(enabled);
        if (enabled) brls::Application::giveFocus(pane);
        else if (brls::Application::getCurrentFocus() == pane) brls::Application::giveFocus(manualButton);
    }
    void toggleFollow() {
        if (!pane->lyrics.synced) return;
        if (pane->manual) setManual(false);
        else pane->follow = !pane->follow;
    }
    void resizeText() { pane->textSize = pane->textSize == 26 ? 30 : pane->textSize == 30 ? 22 : 26; }
public:
    std::function<void()> closed;
    LyricsScreen(std::function<void()> pause, std::function<void()> previous, std::function<void()> next,
        std::function<void()> closed) : pause(std::move(pause)), previous(std::move(previous)), next(std::move(next)), closed(std::move(closed)) {}
    ~LyricsScreen() override { if (closed) closed(); }
    brls::View* createContentView() override {
        auto root = new MusicSurface();
        root->setPadding(20, 32, 12, 32);
        auto header = new brls::Box(brls::Axis::ROW);
        header->setHeight(64); header->setShrink(0);
        brand = label(header, "Navidrome NX", 22, 44); brand->setWidth(416);
        label(header, brls::getStr("app/now_playing"), 20, 44)->setMarginRight(24);
        label(header, brls::getStr("app/lyrics"), 22, 44)->setTextColor(nvgRGB(144, 222, 255));
        header->addView(new brls::Padding());
        button(header, u8"\ue5c4", brls::getStr("app/back"), [] { brls::Application::popActivity(); });
        root->addView(header);
        auto body = new brls::Box(brls::Axis::ROW);
        body->setHeight(0); body->setGrow(1);
        auto player = new brls::Box(brls::Axis::COLUMN);
        player->setWidth(416); player->setShrink(0); player->setPadding(12); player->setMarginRight(24);
        artwork = new brls::Image(); artwork->setDimensions(304, 304); artwork->setShrink(0);
        artwork->setAlignSelf(brls::AlignSelf::CENTER); artwork->setBackgroundColor(nvgRGBA(105, 88, 164, 65));
        player->addView(artwork);
        title = label(player, "", 26, brls::View::AUTO);
        title->setMaxHeight(64);
        title->setSingleLine(false); title->setLineHeight(1.15f);
        title->setVerticalAlign(brls::VerticalAlign::TOP);
        artist = label(player, "", 18, 28); artist->setTextColor(nvgRGB(190, 191, 217));
        title->setWidthPercentage(100); artist->setWidthPercentage(100);
        sourceInfo = label(player, "", 13, 40); sourceInfo->setWidthPercentage(100); sourceInfo->setSingleLine(false);
        sourceInfo->setTextColor(nvgRGB(181, 187, 211));
        auto timeline = new brls::Box(brls::Axis::ROW);
        timeline->setHeight(6); timeline->setShrink(0); timeline->setMarginTop(10);
        timeline->setBackgroundColor(nvgRGBA(183, 173, 221, 70));
        progress = new brls::Rectangle(nvgRGB(120, 221, 250)); progress->setHeight(6); progress->setWidthPercentage(0);
        timeline->addView(progress); player->addView(timeline);
        time = label(player, "0:00", 17, 28);
        time->setWidthPercentage(100);
        auto controls = new brls::Box(brls::Axis::ROW);
        button(controls, u8"\ue045", brls::getStr("app/previous"), previous)->setWidth(80);
        pauseButton = button(controls, u8"\ue037", brls::getStr("app/play"), pause); pauseButton->setWidth(110);
        pauseButton->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        button(controls, u8"\ue044", brls::getStr("app/next"), next)->setWidth(80);
        player->addView(controls);
        state = label(player, "", 14, 24); state->setTextColor(nvgRGB(143, 222, 255));
        state->setWidthPercentage(100);
        body->addView(player);
        auto lyricsPanel = new brls::Box(brls::Axis::COLUMN);
        lyricsPanel->setWidth(0); lyricsPanel->setGrow(1); lyricsPanel->setPadding(24);
        lyricsPanel->setCornerRadius(22); lyricsPanel->setBackgroundColor(nvgRGBA(93, 75, 147, 85));
        lyricsPanel->setBorderThickness(1); lyricsPanel->setBorderColor(nvgRGBA(185, 164, 240, 60));
        label(lyricsPanel, brls::getStr("app/lyrics"), 28, 40)->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        subtitle = label(lyricsPanel, brls::getStr("app/loading_lyrics"), 16, 28);
        subtitle->setTextColor(nvgRGB(187, 190, 216)); subtitle->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        pane = new LyricsPane(); pane->setHeight(0); pane->setGrow(1);
        pane->message = brls::getStr("app/loading_lyrics");
        lyricsPanel->addView(pane);
        auto options = new brls::Box(brls::Axis::ROW); options->setMarginTop(12);
        button(options, u8"\ue245", brls::getStr("app/text_size"), [this] { resizeText(); });
        followButton = button(options, u8"\ue628", brls::getStr("app/follow_off"), [this] { toggleFollow(); });
        manualButton = button(options, u8"\ue8d5", brls::getStr("app/manual_scroll_off"), [this] { setManual(!pane->manual); });
        manualState = label(options, brls::getStr("app/manual_scroll_off"), 16, 44);
        manualState->setWidth(0); manualState->setGrow(1);
        lyricsPanel->addView(options); body->addView(lyricsPanel); root->addView(body);
        auto footer = new brls::Box(brls::Axis::ROW); footer->setHeight(32); footer->setShrink(0);
        label(footer, brls::getStr("app/lyrics_footer"), 16, 32)->setTextColor(nvgRGB(191, 194, 220));
        root->addView(footer);
        return root;
    }
    void onContentAvailable() override {
        registerAction(brls::getStr("app/back"), brls::BUTTON_B, [this](brls::View*) { if (pane->manual) setManual(false); else brls::Application::popActivity(); return true; });
        registerAction(brls::getStr("app/pause"), brls::BUTTON_X, [this](brls::View*) { pause(); return true; });
        registerAction(brls::getStr("app/previous"), brls::BUTTON_LB, [this](brls::View*) { previous(); return true; }, true);
        registerAction(brls::getStr("app/next"), brls::BUTTON_RB, [this](brls::View*) { next(); return true; }, true);
        registerAction(brls::getStr("app/auto_follow"), brls::BUTTON_Y, [this](brls::View*) { toggleFollow(); return true; });
    }
    void focusLyrics() { brls::Application::giveFocus(manualButton); }
    void setTrack(const Entry& track) {
        if (trackId == track.id) return;
        trackId = track.id; artId.clear(); artwork->clearImage(); lastSecond = -1;
        title->setText(track.title);
        const auto details = audioDetails(track);
        sourceInfo->setText((details.empty() ? brls::getStr("app/audio_source_unknown") : brls::getStr("app/audio_source", details))
            + "\n" + brls::getStr("app/playback_mp3", StreamBitRate));
        artist->setText(track.artist.empty() ? brls::getStr("app/unknown_artist") : track.artist);
        setManual(false); pane->lyrics = {}; pane->reset(false); pane->emphasis = {};
        pane->message = brls::getStr("app/loading_lyrics");
        subtitle->setText(brls::getStr("app/loading_lyrics"));
    }
    void setLyrics(SongLyrics lyrics, const std::string& error = "") {
        setManual(false); pane->lyrics = std::move(lyrics); pane->reset(pane->lyrics.synced); pane->emphasis = {};
        pane->message = error.empty() ? brls::getStr("app/no_lyrics") : error;
        subtitle->setText(pane->lyrics.empty() ? pane->message : brls::getStr(pane->lyrics.synced ? "app/synced_lyrics" : "app/plain_lyrics"));
    }
    void update(const Entry& track, long long elapsedMs, bool playing, bool paused, const std::vector<char>* cover) {
        const auto currentClock = preferences.showClock ? clockText() : "";
        if (currentClock != lastClock) { lastClock = currentClock; brand->setText((currentClock.empty() ? "" : currentClock + "   ") + "Navidrome NX"); }
        setTrack(track);
        pane->update(elapsedMs);
        if (lastSecond < 0 || lastManual != pane->manual) {
            lastManual = pane->manual;
            manualButton->setStyle(pane->manual ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
            const auto caption = brls::getStr(pane->manual ? "app/manual_scroll_on" : "app/manual_scroll_off");
            manualButton->updateActionHint(brls::BUTTON_A, caption); manualState->setText(caption);
        }
        if (lastSecond < 0 || lastFollow != pane->follow || lastSynced != pane->lyrics.synced) {
            lastFollow = pane->follow; lastSynced = pane->lyrics.synced;
            followButton->setText(pane->follow ? u8"\ue627" : u8"\ue628");
            followButton->setStyle(pane->follow ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
            followButton->updateActionHint(brls::BUTTON_A, brls::getStr(pane->follow ? "app/follow_on" : "app/follow_off"));
            followButton->setState(pane->lyrics.synced ? brls::ButtonState::ENABLED : brls::ButtonState::DISABLED);
        }
        if (lastSecond != elapsedMs / 1000 || paused != lastPaused || playing != lastPlaying) {
            lastSecond = elapsedMs / 1000; lastPaused = paused; lastPlaying = playing;
            time->setText(playbackTime(lastSecond) + (track.duration > 0 ? " / " + playbackTime(track.duration) : ""));
            progress->setWidthPercentage(track.duration > 0 ? std::min(100.0, elapsedMs / (track.duration * 10.0)) : 0);
            pauseButton->setText(playing && !paused ? u8"\ue034" : u8"\ue037");
            pauseButton->updateActionHint(brls::BUTTON_A, brls::getStr(playing && !paused ? "app/pause" : "app/play"));
            pauseButton->setState(playing ? brls::ButtonState::ENABLED : brls::ButtonState::DISABLED);
            state->setText(brls::getStr(playing ? (paused ? "app/paused" : "app/playing") : "app/stopped"));
        }
        if (cover && !cover->empty() && artId != track.coverArt) {
            artwork->setImageFromMemory(reinterpret_cast<const unsigned char*>(cover->data()), static_cast<int>(cover->size()));
            artId = track.coverArt;
        }
    }
};
