#include <borealis/core/touch.hpp>
#include <cassert>
#include <iostream>
int main() {
    using namespace brls;
    TouchGesture gesture;
    assert(gesture.update({}).phase == TouchPhase::NONE);
    assert(gesture.update({true, 100, 100}).phase == TouchPhase::START);
    assert(gesture.update({true, 104, 105}).phase == TouchPhase::NONE);
    auto tap = gesture.update({});
    assert(tap.phase == TouchPhase::TAP && tap.x == 104 && tap.y == 105);
    assert(gesture.update({}).phase == TouchPhase::NONE);
    gesture.update({true, 100, 100});
    auto drag = gesture.update({true, 100, 125});
    assert(drag.phase == TouchPhase::DRAG && drag.dx == 0 && drag.dy == 25);
    drag = gesture.update({true, 140, 130});
    assert(drag.dx == 0 && drag.dy == 5); // lock to the initial swipe axis
    assert(gesture.update({}).phase == TouchPhase::NONE);
    gesture.update({true, 100, 100});
    drag = gesture.update({true, 50, 105});
    assert(drag.dx == -50 && drag.dy == 0);
    gesture.update({true, 100, 100});
    assert(gesture.update({}).phase == TouchPhase::NONE); // returning to start is still a swipe
    gesture.update({true, 100, 100}); gesture.cancel();
    assert(gesture.update({}).phase == TouchPhase::NONE);
    gesture.update({true, 100, 100});
    assert(gesture.update({false, 0, 0, true}).phase == TouchPhase::NONE);
    assert(gesture.update({true, 100, 100}).phase == TouchPhase::NONE);
    assert(gesture.update({}).phase == TouchPhase::NONE);
    assert(gesture.update({true, 100, 100}).phase == TouchPhase::START);
    assert(gesture.update({}).phase == TouchPhase::TAP);
    std::cout << "Touch taps, swipe threshold, axis lock and cancellation checks passed\n";
}
