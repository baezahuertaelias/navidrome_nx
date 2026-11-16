#pragma once
#include <aurore.hpp>
#include <aurore/cadran.hpp>
#include <aurore/cadran/ui/cadran_kit.hpp>
#include <aurore/core/touch/pan_gesture.hpp>
#include <aurore/core/touch/tap_gesture.hpp>
#include "artwork.hpp"

// App captions can change between text and a single Material glyph during playback.
class MusicButton : public aurore::IconButton {
    std::string value;
    bool glyph = false;
public:
    void setText(const std::string& text) {
        if (value == text) return;
        value = text;
        const bool icon = text.size() == 3 && static_cast<unsigned char>(text[0]) == 0xee;
        if (icon) { setIconSize(26); setGlyph(text); aurore::IconButton::setText(""); setPadding(6); }
        else { if (glyph) setIconView(nullptr); aurore::IconButton::setText(text); }
        glyph = icon;
    }
    void updateActionHint(aurore::ControllerButton button, const std::string& caption) {
        aurore::IconButton::updateActionHint(button, caption);
        if (button == aurore::BUTTON_A) setAccessibilityLabel(caption);
    }
};
inline aurore::Box* musicCard(aurore::Axis axis, bool focusable = false) {
    auto card = cadran::kit::card(focusable, false); card->setAxis(axis);
    if (focusable) card->addGestureRecognizer(new aurore::TapGestureRecognizer(card));
    return card;
}
inline void musicPan(aurore::View* view, aurore::PanAxis axis, std::function<void(float, float)> callback) {
    view->addGestureRecognizer(new aurore::PanGestureRecognizer([callback](aurore::PanGestureStatus state, aurore::Sound*) {
        if (state.state == aurore::GestureState::START || state.state == aurore::GestureState::STAY)
            callback(-state.delta.x, -state.delta.y);
    }, axis));
}
inline bool loadCover(aurore::Image* image, const unsigned char* bytes, int size) {
    int width = 0, height = 0;
    if (!coverDimensions(bytes, size, width, height)) return false;
    image->setImageFromMem(bytes, size);
    return image->getTexture() != 0;
}
