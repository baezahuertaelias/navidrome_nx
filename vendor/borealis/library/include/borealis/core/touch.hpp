#pragma once
#include <cmath>
namespace brls {
struct TouchState { bool pressed = false; float x = 0, y = 0; bool cancelled = false; };
enum class TouchPhase { NONE, START, DRAG, TAP };
struct TouchEvent { TouchPhase phase = TouchPhase::NONE; float x = 0, y = 0, dx = 0, dy = 0; };
class TouchGesture {
    bool held = false, dragged = false, horizontal = false, blocked = false;
    float startX = 0, startY = 0, lastX = 0, lastY = 0;
public:
    void cancel() { held = false; dragged = true; blocked = true; }
    TouchEvent update(TouchState state) {
        if (state.cancelled) { cancel(); return {}; }
        if (blocked) { if (!state.pressed) blocked = false; return {}; }
        TouchEvent event; event.x = state.pressed ? state.x : lastX; event.y = state.pressed ? state.y : lastY;
        if (state.pressed && !held) {
            held = true; dragged = false; startX = lastX = state.x; startY = lastY = state.y; event.phase = TouchPhase::START;
        } else if (state.pressed) {
            if (!dragged && std::hypot(state.x - startX, state.y - startY) >= 12) {
                dragged = true; horizontal = std::abs(state.x - startX) > std::abs(state.y - startY);
            }
            if (dragged) {
                event.phase = TouchPhase::DRAG;
                event.dx = horizontal ? state.x - lastX : 0;
                event.dy = horizontal ? 0 : state.y - lastY;
            }
            lastX = state.x; lastY = state.y;
        } else if (held) {
            held = false;
            if (!dragged) event.phase = TouchPhase::TAP;
        }
        return event;
    }
};
}
