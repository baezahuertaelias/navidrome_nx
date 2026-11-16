#pragma once
#include "music_ui.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <memory>
#include "api.hpp"
#include "cover_flow_layout.hpp"

class CoverFlow : public aurore::Box {
    struct Cover { std::string id; std::unique_ptr<aurore::Image> image; };
    std::array<Cover, CoverFlowRadius * 2 + 1> covers;
    std::shared_ptr<const std::vector<Entry>> entries;
    size_t selected = 0;
    float position = 0, swipe = 0;
    std::function<void(size_t)> activate, selectedChanged;
    std::function<void(std::vector<std::pair<std::string, aurore::Image*>>)> coversChanged;
    aurore::Label *title, *artist, *counter;
    std::chrono::steady_clock::time_point lastFrame = std::chrono::steady_clock::now();
    void updateSelection() {
        std::vector<std::string> needed;
        for (int offset : {0, -1, 1, -2, 2, -3, 3}) {
            const auto index = static_cast<long long>(selected) + offset;
            if (index < 0 || static_cast<size_t>(index) >= entries->size()) continue;
            const auto& id = (*entries)[index].coverArt;
            if (!id.empty() && std::find(needed.begin(), needed.end(), id) == needed.end()) needed.push_back(id);
        }
        std::vector<std::pair<std::string, aurore::Image*>> targets;
        for (const auto& id : needed) {
            auto slot = std::find_if(covers.begin(), covers.end(), [&](const Cover& cover) { return cover.id == id; });
            if (slot == covers.end()) {
                slot = std::find_if(covers.begin(), covers.end(), [&](const Cover& cover) { return std::find(needed.begin(), needed.end(), cover.id) == needed.end(); });
                assert(slot != covers.end());
                slot->image->clear(); slot->id = id;
            }
            targets.push_back({id, slot->image.get()});
        }
        coversChanged(std::move(targets));
        if (entries->empty()) return;
        const auto& entry = (*entries)[selected];
        title->setText(entry.title);
        artist->setText(entry.kind == EntryKind::Artist ? aurore::getStr("app/view_albums") : entry.kind == EntryKind::Playlist ? aurore::getStr("app/playlist_tracks", entry.songCount) : entry.artist.empty() ? aurore::getStr("app/unknown_artist") : entry.artist);
        counter->setText(aurore::getStr("app/coverflow_position", selected + 1, entries->size()));
        selectedChanged(selected);
    }
    bool move(int direction) {
        const auto next = coverFlowMove(selected, entries->size(), direction);
        if (next == selected) return false;
        selected = next; updateSelection(); return true;
    }
    void paintCover(NVGcontext* vg, size_t index, float centerX, float centerY, float art, float alpha) {
        const auto pose = coverFlowPose(static_cast<float>(index) - position, art);
        const auto& entry = (*entries)[index];
        auto image = std::find_if(covers.begin(), covers.end(), [&](const Cover& cover) { return !entry.coverArt.empty() && cover.id == entry.coverArt; });
        nvgSave(vg); nvgTranslate(vg, centerX + pose.center, centerY);
        nvgTransform(vg, pose.scaleX, pose.shear, 0, pose.scaleY, 0, 0);
        nvgBeginPath(vg); nvgRoundedRect(vg, -art / 2, -art / 2, art, art, 8);
        if (image != covers.end() && image->image->getTexture()) {
            const float originalWidth = image->image->getOriginalImageWidth(), originalHeight = image->image->getOriginalImageHeight();
            const float scale = art / std::max(1.0f, std::min(originalWidth, originalHeight));
            const float imageWidth = originalWidth * scale, imageHeight = originalHeight * scale;
            nvgFillPaint(vg, nvgImagePattern(vg, -imageWidth / 2, -imageHeight / 2, imageWidth, imageHeight, 0, image->image->getTexture(), alpha * pose.alpha));
        } else nvgFillColor(vg, nvgRGBA(67, 60, 104, static_cast<unsigned char>(alpha * pose.alpha * 230)));
        nvgFill(vg);
        if (index == selected && alpha > 0.5f && isFocused()) {
            nvgStrokeWidth(vg, 2); nvgStrokeColor(vg, nvgRGBA(116, 220, 255, 220)); nvgStroke(vg);
        }
        if (image == covers.end() || !image->image->getTexture()) {
            nvgFontFaceId(vg, aurore::Application::getFont(aurore::FONT_MATERIAL_ICONS));
            nvgFontSize(vg, art * 0.23f); nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGBA(187, 176, 223, static_cast<unsigned char>(alpha * 200)));
            const char* icon = entry.kind == EntryKind::Artist ? "\ue7fd" : entry.kind == EntryKind::Playlist ? "\ue05f" : "\ue019";
            nvgText(vg, 0, 0, icon, nullptr);
        }
        nvgRestore(vg);
    }
public:
    CoverFlow(std::shared_ptr<const std::vector<Entry>> entries, size_t selected, float height,
        std::function<void(std::vector<std::pair<std::string, aurore::Image*>>)> coversChanged,
        std::function<void(size_t)> selectedChanged, std::function<void(size_t)> activate)
        : aurore::Box(aurore::Axis::COLUMN), entries(std::move(entries)), activate(std::move(activate)), selectedChanged(std::move(selectedChanged)), coversChanged(std::move(coversChanged)) {
        setId("library/coverflow"); setAccessibilityLabel(aurore::getStr("app/view_coverflow"));
        this->selected = coverFlowMove(selected, this->entries->size(), 0); position = this->selected;
        setHeight(std::max(200.0f, height)); setShrink(0); setWidthPercentage(100); setPadding(8, 12, 8, 12);
        musicPan(this, aurore::PanAxis::HORIZONTAL, [this](float dx, float dy) { onTouchScroll(dx, dy); });
        addGestureRecognizer(new aurore::TapGestureRecognizer([this](aurore::TapGestureStatus state, aurore::Sound*) {
            if (state.state == aurore::GestureState::END) onTouchTap(state.position.x, state.position.y);
        }));
        setFocusable(!this->entries->empty()); setHideHighlight(true); setHideHighlightBackground(true);
        for (auto& cover : covers) { cover.image = std::make_unique<aurore::Image>(); cover.image->setDimensions(256, 256); cover.image->detach(); }
        auto spacer = new aurore::Padding(); spacer->setHeight(0); spacer->setGrow(1); addView(spacer);
        auto caption = [this](int size, float height) {
            auto label = new aurore::Label(); label->setFontSize(size); label->setHeight(height); label->setShrink(0);
            label->setWidthPercentage(100); label->setSingleLine(true); label->setHorizontalAlign(aurore::HorizontalAlign::CENTER);
            addView(label); return label;
        };
        title = caption(23, 30); title->setId("coverflow/title"); artist = caption(17, 25); counter = caption(14, 23);
        artist->setTextColor(nvgRGB(190, 190, 216)); counter->setTextColor(nvgRGB(145, 185, 215));
        registerAction("", aurore::BUTTON_NAV_LEFT, [this](aurore::View*) { return move(-1); }, true, true);
        registerAction("", aurore::BUTTON_NAV_RIGHT, [this](aurore::View*) { return move(1); }, true, true);
        registerClickAction([this](aurore::View*) { if (!this->entries->empty()) this->activate(this->selected); return true; });
        updateSelection();
    }
    aurore::View* hitTest(aurore::Point point) override { return aurore::View::hitTest(point); }
    bool onTouchScroll(float dx, float) {
        if (!dx || entries->empty()) return false;
        swipe += dx;
        while (std::abs(swipe) >= 60) {
            const int direction = swipe < 0 ? 1 : -1;
            move(direction); swipe += direction * 60;
        }
        return true;
    }
    bool onTouchTap(float x, float y) {
        const float art = coverFlowArtSize(getWidth(), getHeight()), centerY = 12 + art / 2;
        for (int offset : {0, -1, 1, -2, 2, -3, 3}) {
            const auto index = static_cast<long long>(selected) + offset;
            if (index < 0 || static_cast<size_t>(index) >= entries->size()) continue;
            const auto pose = coverFlowPose(static_cast<float>(index) - position, art);
            if (!coverFlowHit(x - getX() - getWidth() / 2, y - getY(), centerY, art, pose)) continue;
            if (static_cast<size_t>(index) == selected) { auto action = activate; action(selected); }
            else { selected = index; updateSelection(); }
            break;
        }
        return true;
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height, aurore::Style style, aurore::FrameContext* context) override {
        const auto now = std::chrono::steady_clock::now();
        const float seconds = std::min(0.1f, std::chrono::duration<float>(now - lastFrame).count()); lastFrame = now;
        position += (selected - position) * (1 - std::exp(-seconds * 14));
        for (auto& cover : covers) cover.image->setDetachedPosition(x, y);
        nvgSave(vg); nvgIntersectScissor(vg, x, y, width, height);
        const float art = coverFlowArtSize(width, height), centerY = y + 12 + art / 2;
        const auto visible = coverFlowDrawOrder(selected, entries->size(), position);
        for (auto index : visible) {
            if (index >= entries->size()) continue;
            // A short mirrored reflection fades into the background below each cover.
            const float floor = centerY + art / 2 + 5;
            for (int band = 0; band < 4; ++band) {
                nvgSave(vg); nvgIntersectScissor(vg, x, floor + band * art * 0.05f, width, art * 0.05f);
                nvgTranslate(vg, 0, 2 * floor); nvgScale(vg, 1, -1);
                paintCover(vg, index, x + width / 2, centerY, art, 0.12f * (4 - band) / 4); nvgRestore(vg);
            }
            paintCover(vg, index, x + width / 2, centerY, art, 1);
        }
        aurore::Box::draw(vg, x, y, width, height, style, context); nvgRestore(vg);
    }
};
