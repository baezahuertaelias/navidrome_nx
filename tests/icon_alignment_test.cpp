#include <nanovg.h>
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    NVGparams params{};
    params.renderCreate = [](void*) { return 1; };
    params.renderCreateTexture = [](void*, int, int, int, int, const unsigned char*) { return 1; };
    params.renderDeleteTexture = [](void*, int) { return 1; };
    params.renderViewport = [](void*, float, float, float) {};
    auto vg = nvgCreateInternal(&params); assert(vg);
    const auto regular = nvgCreateFont(vg, "music", "resources/inter/NavidromeMusic.ttf");
    const auto material = nvgCreateFont(vg, "icons", "resources/material/MaterialIcons-Regular.ttf");
    assert(regular >= 0 && material >= 0); nvgAddFallbackFontId(vg, regular, material);
    nvgBeginFrame(vg, 1280, 720, 1);
    nvgFontFaceId(vg, regular); nvgFontSize(vg, 26); nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
    bool correctedLineMetrics = false;
    for (const auto* icon : {u8"\ue034",u8"\ue037",u8"\ue044",u8"\ue045",u8"\ue047",u8"\ue244",u8"\ue245",u8"\ue627",u8"\ue628",u8"\ue5c4"}) {
        float ink[4], line[4], centered[4];
        nvgTextGlyphBounds(vg, 0, 0, icon, nullptr, ink); nvgTextBounds(vg, 0, 0, icon, nullptr, line);
        assert(ink[2] > ink[0] && ink[3] > ink[1]);
        if (std::abs(ink[1] + ink[3] - line[1] - line[3]) > 1) correctedLineMetrics = true;
        const float x = (64 - ink[2] - ink[0]) / 2, y = (44 - ink[3] - ink[1]) / 2;
        nvgTextGlyphBounds(vg, x, y, icon, nullptr, centered);
        assert(std::abs((centered[0] + centered[2]) / 2 - 32) <= 1);
        assert(std::abs((centered[1] + centered[3]) / 2 - 22) <= 1);
    }
    assert(correctedLineMetrics);
    nvgDeleteInternal(vg);
    std::cout << "Actual fallback-font icon bounds center within one pixel\n";
}
