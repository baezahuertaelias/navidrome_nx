#pragma once
#include <functional>
#include "api.hpp"
#include "music_surface.hpp"
#include "playback_clock.hpp"
#include "clock_text.hpp"
#include "lyric_emphasis.hpp"
#include "audio_details.hpp"
#include "lyrics_navigation.hpp"

class LyricsPane : public aurore::View, public LyricsNavigation {
public:
    SongLyrics lyrics;
    int textSize = 26;
    std::string message;
    LyricEmphasis emphasis;

    LyricsPane() {
        setFocusable(false);
        setAccessibilityLabel(aurore::getStr("app/lyrics"));
        musicPan(this, aurore::PanAxis::VERTICAL, [this](float, float dy) { scroll(dy, lyrics.lines.size()); });
        setHideHighlightBackground(true);
        setHideHighlight(true);
        setHighlightCornerRadius(14);
        registerAction("", aurore::BUTTON_NAV_UP, [this](aurore::View*) { move(-1); return true; }, true, true);
        registerAction("", aurore::BUTTON_NAV_DOWN, [this](aurore::View*) { move(1); return true; }, true, true);
    }
    bool onTouchScroll(float, float dy) { return scroll(dy, lyrics.lines.size()); }
    void move(int direction) { LyricsNavigation::move(direction, lyrics.lines.size()); }
    void update(long long elapsedMs) {
        updateActive(lyrics.activeLine(elapsedMs));
        emphasis.update(active, std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        aurore::Style, aurore::FrameContext*) override {
        nvgSave(vg);
        nvgIntersectScissor(vg, x, y, width, height);
        nvgFontFaceId(vg, aurore::Application::getFont(aurore::FONT_REGULAR));
        nvgFontSize(vg, textSize);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgTextLineHeight(vg, 1.3f);
        if (lyrics.empty()) {
            nvgFillColor(vg, nvgRGB(186, 192, 219));
            nvgTextBox(vg, x + 20, y + 32, width - 48, message.c_str(), nullptr);
        } else {
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            const int regular = aurore::Application::getFont(aurore::FONT_REGULAR), bold = aurore::Application::getFont("lyrics-bold");
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

class LyricsScreen : public aurore::Activity {
    aurore::Image* artwork = nullptr;
    aurore::Label *title = nullptr, *artist = nullptr, *time = nullptr, *state = nullptr, *subtitle = nullptr, *brand = nullptr;
    aurore::Rectangle* progress = nullptr;
    aurore::Label* sourceInfo = nullptr;
    MusicButton *pauseButton = nullptr, *followButton = nullptr, *manualButton = nullptr;
    aurore::Label* manualState = nullptr;
    LyricsPane* pane = nullptr;
    std::string trackId, artId;
    long long lastSecond = -1;
    bool lastPaused = false, lastPlaying = false;
    bool lastFollow = false, lastSynced = false, lastManual = false;
    std::string lastClock;
    std::function<void()> pause, previous, next;

    aurore::Label* label(aurore::Box* parent, const std::string& text, int size, float height) {
        auto view = new aurore::Label();
        view->setText(text); view->setFontSize(size); view->setHeight(height);
        view->setSingleLine(true); view->setShrink(0);
        parent->addView(view); return view;
    }
    MusicButton* button(aurore::Box* parent, const std::string& icon, const std::string& caption, std::function<void()> callback) {
        auto view = new MusicButton();
        view->applyXMLAttribute("fontSize", "26");
        view->setText(icon); view->setHeight(44); view->setWidth(64); view->setShrink(0);
        view->setPadding(8, 14, 8, 14); view->setMarginRight(10); view->setCornerRadius(12);
        view->registerClickAction([callback](aurore::View*) { callback(); return true; });
        view->updateActionHint(aurore::BUTTON_A, caption);
        parent->addView(view); return view;
    }
    void setManual(bool enabled) {
        const bool focused = aurore::Application::getCurrentFocus() == pane;
        pane->setManual(enabled, pane->lyrics.synced);
        pane->setFocusable(enabled);
        if (enabled) aurore::Application::giveFocus(pane);
        else if (focused) aurore::Application::giveFocus(manualButton);
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
    aurore::View* createContentView() override {
        auto root = new MusicSurface();
        root->setPadding(20, 32, 12, 32);
        auto header = new aurore::Box(aurore::Axis::ROW);
        header->setHeight(64); header->setShrink(0);
        brand = label(header, "Navidrome NX", 22, 44); brand->setWidth(416);
        label(header, aurore::getStr("app/now_playing"), 20, 44)->setMarginRight(24);
        label(header, aurore::getStr("app/lyrics"), 22, 44)->setTextColor(nvgRGB(144, 222, 255));
        header->addView(new aurore::Padding());
        button(header, "\ue5c4", aurore::getStr("app/back"), [] { aurore::Application::popActivity(); });
        root->addView(header);
        auto body = new aurore::Box(aurore::Axis::ROW);
        body->setHeight(0); body->setGrow(1);
        auto player = new aurore::Box(aurore::Axis::COLUMN);
        player->setWidth(416); player->setShrink(0); player->setPadding(12); player->setMarginRight(24);
        artwork = new aurore::Image(); artwork->setWidth(304); artwork->setClipsToBounds(false); artwork->setHeight(0); artwork->setGrow(1); artwork->setShrink(1); artwork->setMinHeight(120); artwork->setMaxHeight(304);
        artwork->setAlignSelf(aurore::AlignSelf::CENTER); artwork->setBackgroundColor(nvgRGBA(105, 88, 164, 65));
        player->addView(artwork);
        title = label(player, "", 26, aurore::View::AUTO);
        title->setMaxHeight(64);
        title->setSingleLine(false); title->setLineHeight(1.15f);
        title->setVerticalAlign(aurore::VerticalAlign::TOP);
        artist = label(player, "", 18, 28); artist->setTextColor(nvgRGB(190, 191, 217));
        title->setWidthPercentage(100); artist->setWidthPercentage(100);
        sourceInfo = label(player, "", 13, 40); sourceInfo->setWidthPercentage(100); sourceInfo->setSingleLine(false);
        sourceInfo->setTextColor(nvgRGB(181, 187, 211));
        auto timeline = new aurore::Box(aurore::Axis::ROW);
        timeline->setHeight(6); timeline->setShrink(0); timeline->setMarginTop(10);
        timeline->setBackgroundColor(nvgRGBA(183, 173, 221, 70));
        progress = new aurore::Rectangle(nvgRGB(120, 221, 250)); progress->setHeight(6); progress->setWidthPercentage(0);
        timeline->addView(progress); player->addView(timeline);
        time = label(player, "0:00", 17, 28);
        time->setWidthPercentage(100);
        auto controls = new aurore::Box(aurore::Axis::ROW);
        button(controls, "\ue045", aurore::getStr("app/previous"), previous)->setWidth(80);
        pauseButton = button(controls, "\ue037", aurore::getStr("app/play"), pause); pauseButton->setWidth(110);
        pauseButton->setId("lyrics/pause"); pauseButton->setStyle(&aurore::BUTTONSTYLE_PRIMARY);
        button(controls, "\ue044", aurore::getStr("app/next"), next)->setWidth(80);
        player->addView(controls);
        state = label(player, "", 14, 24); state->setTextColor(nvgRGB(143, 222, 255));
        state->setWidthPercentage(100);
        body->addView(player);
        auto lyricsPanel = musicCard(aurore::Axis::COLUMN);
        lyricsPanel->setWidth(0); lyricsPanel->setGrow(1); lyricsPanel->setPadding(24);
        lyricsPanel->setCornerRadius(22); lyricsPanel->setBackgroundColor(nvgRGBA(93, 75, 147, 85));
        lyricsPanel->setBorderThickness(1); lyricsPanel->setBorderColor(nvgRGBA(185, 164, 240, 60));
        label(lyricsPanel, aurore::getStr("app/lyrics"), 28, 40)->setHorizontalAlign(aurore::HorizontalAlign::CENTER);
        subtitle = label(lyricsPanel, aurore::getStr("app/loading_lyrics"), 16, 28);
        subtitle->setTextColor(nvgRGB(187, 190, 216)); subtitle->setHorizontalAlign(aurore::HorizontalAlign::CENTER);
        pane = new LyricsPane(); pane->setId("lyrics/pane"); pane->setHeight(0); pane->setGrow(1);
        pane->message = aurore::getStr("app/loading_lyrics");
        lyricsPanel->addView(pane);
        auto options = new aurore::Box(aurore::Axis::ROW); options->setMarginTop(12);
        button(options, "\ue245", aurore::getStr("app/text_size"), [this] { resizeText(); });
        followButton = button(options, "\ue628", aurore::getStr("app/follow_off"), [this] { toggleFollow(); });
        manualButton = button(options, "\ue8d5", aurore::getStr("app/manual_scroll_off"), [this] { setManual(!pane->manual); });
        manualButton->setId("lyrics/manual");
        manualState = label(options, aurore::getStr("app/manual_scroll_off"), 16, 44);
        manualState->setWidth(0); manualState->setGrow(1);
        followButton->setId("lyrics/follow");
        lyricsPanel->addView(options); body->addView(lyricsPanel); root->addView(body);
        auto footer = new aurore::Box(aurore::Axis::ROW); footer->setHeight(32); footer->setShrink(0);
        label(footer, aurore::getStr("app/lyrics_footer"), 16, 32)->setTextColor(nvgRGB(191, 194, 220));
        root->addView(footer);
        return root;
    }
    void onContentAvailable() override {
        registerAction(aurore::getStr("app/back"), aurore::BUTTON_B, [this](aurore::View*) { if (pane->manual) setManual(false); else aurore::Application::popActivity(); return true; });
        registerAction(aurore::getStr("app/pause"), aurore::BUTTON_X, [this](aurore::View*) { pause(); return true; });
        registerAction(aurore::getStr("app/previous"), aurore::BUTTON_LB, [this](aurore::View*) { previous(); return true; }, true, true);
        registerAction(aurore::getStr("app/next"), aurore::BUTTON_RB, [this](aurore::View*) { next(); return true; }, true, true);
        registerAction(aurore::getStr("app/auto_follow"), aurore::BUTTON_Y, [this](aurore::View*) { toggleFollow(); return true; });
    }
    void focusLyrics() { aurore::Application::giveFocus(manualButton); }
    void setTrack(const Entry& track) {
        if (trackId == track.id) return;
        trackId = track.id; artId.clear(); artwork->clear(); lastSecond = -1;
        title->setText(track.title);
        const auto details = audioDetails(track);
        sourceInfo->setText((details.empty() ? aurore::getStr("app/audio_source_unknown") : aurore::getStr("app/audio_source", details))
            + "\n" + aurore::getStr("app/playback_mp3", StreamBitRate));
        artist->setText(track.artist.empty() ? aurore::getStr("app/unknown_artist") : track.artist);
        setManual(false); pane->lyrics = {}; pane->reset(false); pane->emphasis = {};
        pane->message = aurore::getStr("app/loading_lyrics");
        subtitle->setText(aurore::getStr("app/loading_lyrics"));
    }
    void setLyrics(SongLyrics lyrics, const std::string& error = "") {
        setManual(false); pane->lyrics = std::move(lyrics); pane->reset(pane->lyrics.synced); pane->emphasis = {};
        pane->message = error.empty() ? aurore::getStr("app/no_lyrics") : error;
        subtitle->setText(pane->lyrics.empty() ? pane->message : aurore::getStr(pane->lyrics.synced ? "app/synced_lyrics" : "app/plain_lyrics"));
    }
    void update(const Entry& track, long long elapsedMs, bool playing, bool paused, const std::vector<char>* cover) {
        const auto currentClock = preferences.showClock ? clockText() : "";
        if (currentClock != lastClock) { lastClock = currentClock; brand->setText((currentClock.empty() ? "" : currentClock + "   ") + "Navidrome NX"); }
        setTrack(track);
        pane->update(elapsedMs);
        if (lastSecond < 0 || lastManual != pane->manual) {
            lastManual = pane->manual;
            manualButton->setStyle(pane->manual ? &aurore::BUTTONSTYLE_PRIMARY : &aurore::BUTTONSTYLE_DEFAULT);
            const auto caption = aurore::getStr(pane->manual ? "app/manual_scroll_on" : "app/manual_scroll_off");
            manualButton->updateActionHint(aurore::BUTTON_A, caption); manualState->setText(caption);
        }
        if (lastSecond < 0 || lastFollow != pane->follow || lastSynced != pane->lyrics.synced) {
            lastFollow = pane->follow; lastSynced = pane->lyrics.synced;
            followButton->setText(pane->follow ? "\ue627" : "\ue628");
            followButton->setStyle(pane->follow ? &aurore::BUTTONSTYLE_PRIMARY : &aurore::BUTTONSTYLE_DEFAULT);
            followButton->updateActionHint(aurore::BUTTON_A, aurore::getStr(pane->follow ? "app/follow_on" : "app/follow_off"));
            followButton->setState(pane->lyrics.synced ? aurore::ButtonState::ENABLED : aurore::ButtonState::DISABLED);
        }
        if (lastSecond != elapsedMs / 1000 || paused != lastPaused || playing != lastPlaying) {
            lastSecond = elapsedMs / 1000; lastPaused = paused; lastPlaying = playing;
            time->setText(playbackTime(lastSecond) + (track.duration > 0 ? " / " + playbackTime(track.duration) : ""));
            progress->setWidthPercentage(track.duration > 0 ? std::min(100.0, elapsedMs / (track.duration * 10.0)) : 0);
            pauseButton->setText(playing && !paused ? "\ue034" : "\ue037");
            pauseButton->updateActionHint(aurore::BUTTON_A, aurore::getStr(playing && !paused ? "app/pause" : "app/play"));
            pauseButton->setState(playing ? aurore::ButtonState::ENABLED : aurore::ButtonState::DISABLED);
            state->setText(aurore::getStr(playing ? (paused ? "app/paused" : "app/playing") : "app/stopped"));
        }
        if (cover && !cover->empty() && artId != track.coverArt) {
            loadCover(artwork, reinterpret_cast<const unsigned char*>(cover->data()), static_cast<int>(cover->size()));
            artId = track.coverArt;
        }
    }
};
