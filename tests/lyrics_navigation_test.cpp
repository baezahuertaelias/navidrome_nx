#include "lyrics_navigation.hpp"
#include <cassert>
#include <iostream>
int main() {
    LyricsNavigation navigation;
    navigation.reset(true); navigation.updateActive(3);
    assert(!navigation.manual && navigation.follow && navigation.selected == 3);
    navigation.move(1, 10); assert(navigation.selected == 3);
    assert(!navigation.scroll(-144, 10) && navigation.touchOffset == 0);
    navigation.setManual(true, true);
    assert(navigation.manual && !navigation.follow);
    navigation.move(1, 10); assert(navigation.selected == 4);
    navigation.updateActive(6); assert(navigation.selected == 4);
    assert(navigation.scroll(-72, 10) && navigation.selected == 6);
    navigation.move(100, 10); assert(navigation.selected == 9);
    navigation.setManual(false, true);
    assert(!navigation.manual && navigation.follow && navigation.selected == 6);
    assert(!navigation.scroll(72, 10));
    navigation.updateActive(7); assert(navigation.selected == 7);
    navigation.reset(false); navigation.move(1, 10);
    assert(!navigation.manual && !navigation.follow && navigation.selected == 0);
    navigation.setManual(true, false); navigation.move(2, 10); assert(navigation.selected == 2);
    navigation.setManual(false, false); assert(navigation.selected == 0 && !navigation.follow);
    navigation.reset(false); navigation.setManual(true, false);
    assert(!navigation.scroll(100, 0)); navigation.move(1, 0); assert(navigation.selected == 0);
    std::cout << "Lyrics manual-scroll lock, touch gating, follow and reset checks passed\n";
}
