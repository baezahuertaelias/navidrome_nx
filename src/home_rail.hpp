#pragma once
#include "home_data.hpp"

class HomeRail : public brls::Box {
    brls::Box* cards;
    size_t selected = 0;
    float offset = 0;
    bool touchScrolling = false;
    void reveal() {
        offset = homeRailOffset(selected, cards->getChildren().size(), getWidth(), offset);
        cards->setTranslationX(-offset);
    }
public:
    HomeRail(size_t count) : brls::Box(brls::Axis::COLUMN) {
        setHeight(256); setShrink(0);
        cards = new brls::Box(brls::Axis::ROW);
        cards->setWidth(HomeRailPadding * 2 + count * HomeCardWidth + (count ? count - 1 : 0) * HomeCardGap);
        cards->setHeight(256); cards->setShrink(0); cards->setPadding(HomeRailPadding);
        brls::Box::addView(cards);
    }
    void addCard(brls::View* card) { cards->addView(card); }
    bool onTouchScroll(float dx, float) override {
        if (!dx) return false;
        touchScrolling = true;
        offset = std::clamp(offset - dx, 0.0f, std::max(0.0f, cards->getWidth() - getWidth()));
        cards->setTranslationX(-offset); return true;
    }
    void onChildFocusGained(brls::View* direct, brls::View* focused) override {
        touchScrolling = false;
        const auto children = cards->getChildren();
        auto found = std::find(children.begin(), children.end(), focused);
        if (found != children.end()) selected = static_cast<size_t>(std::distance(children.begin(), found));
        reveal();
        brls::Box::onChildFocusGained(direct, focused);
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* context) override {
        if (!touchScrolling) reveal();
        nvgSave(vg); nvgIntersectScissor(vg, x, y, width, height);
        brls::Box::draw(vg, x, y, width, height, style, context); nvgRestore(vg);
    }
};
