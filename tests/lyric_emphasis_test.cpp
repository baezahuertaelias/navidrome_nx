#include "lyric_emphasis.hpp"
#include <cassert>
#include <cmath>
int main() {
    LyricEmphasis emphasis;
    assert(emphasis.amount(0, 0) == 0 && emphasis.amount(-1, 0) == 0);
    emphasis.update(0, 10);
    assert(emphasis.amount(0, 10) == 0);
    assert(std::abs(emphasis.amount(0, 10.125) - 0.5f) < 0.0001f);
    assert(emphasis.amount(0, 10.25) == 1);
    emphasis.update(1, 11);
    assert(emphasis.amount(0, 11) == 1 && emphasis.amount(1, 11) == 0);
    assert(std::abs(emphasis.amount(0, 11.125) - 0.5f) < 0.0001f);
    assert(emphasis.amount(0, 11.25) == 0 && emphasis.amount(1, 11.25) == 1);
    emphasis.update(2, 12);
    const auto outgoing = emphasis.amount(2, 12.05);
    emphasis.update(3, 12.05);
    assert(std::abs(emphasis.amount(2, 12.05) - outgoing) < 0.0001f);
    for (double seconds : {12.05, 12.10, 12.20, 12.30}) {
        for (int line = 0; line < 5; ++line) {
            const auto weight = emphasis.amount(line, seconds);
            assert(weight >= 0 && weight <= 1);
        }
    }
    emphasis.update(-1, 13);
    assert(emphasis.amount(3, 13.25) == 0);
    emphasis = {}; assert(emphasis.amount(0, 100) == 0);
}
