#pragma once
#include "home_data.hpp"
#include "music_ui.hpp"

class HomeRail : public aurore::Box {
    aurore::Box* cards;
    size_t selected = 0;
    float offset = 0;
    bool touchScrolling = false;
    void reveal() {
        offset = homeRailOffset(selected, cards->getChildren().size(), getWidth(), offset);
        cards->setTranslationX(-offset);
    }
public:
    HomeRail(size_t count) : aurore::Box(aurore::Axis::COLUMN) {
        musicPan(this, aurore::PanAxis::HORIZONTAL, [this](float dx, float dy) { onTouchScroll(dx, dy); });
        setHeight(256); setShrink(0);
        cards = new aurore::Box(aurore::Axis::ROW);
        cards->setWidth(HomeRailPadding * 2 + count * HomeCardWidth + (count ? count - 1 : 0) * HomeCardGap);
        cards->setHeight(256); cards->setShrink(0); cards->setPadding(HomeRailPadding);
        aurore::Box::addView(cards);
    }
    void addCard(aurore::View* card) { cards->addView(card); }
    bool onTouchScroll(float dx, float) {
        if (!dx) return false;
        touchScrolling = true;
        offset = std::clamp(offset - dx, 0.0f, std::max(0.0f, cards->getWidth() - getWidth()));
        cards->setTranslationX(-offset); return true;
    }
    void onChildFocusGained(aurore::View* direct, aurore::View* focused) override {
        touchScrolling = false;
        const auto children = cards->getChildren();
        auto found = std::find(children.begin(), children.end(), focused);
        if (found != children.end()) selected = static_cast<size_t>(std::distance(children.begin(), found));
        reveal();
        aurore::Box::onChildFocusGained(direct, focused);
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        aurore::Style style, aurore::FrameContext* context) override {
        if (!touchScrolling) reveal();
        nvgSave(vg); nvgIntersectScissor(vg, x, y, width, height);
        aurore::Box::draw(vg, x, y, width, height, style, context); nvgRestore(vg);
    }
};
