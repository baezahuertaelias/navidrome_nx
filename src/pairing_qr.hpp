#pragma once
#include <cmath>
#include <algorithm>
#include "pairing_code.hpp"

class PairingQr : public brls::View {
    uint8_t modules[PairingQrBytes]{};
    std::string payload;
    bool valid = false;
public:
    PairingQr() { setDimensions(264, 264); setShrink(0); }
    void update(const std::string& value) {
        if (value == payload) return;
        payload = value;
        valid = encodePairingQr(value, modules);
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style, brls::FrameContext*) override {
        if (!valid) return;
        const int size = qrcodegen_getSize(modules);
        const float step = std::floor(std::min(width, height) / (size + 8));
        const float left = x + (width - (size + 8) * step) / 2;
        const float top = y + (height - (size + 8) * step) / 2;
        nvgSave(vg); nvgShapeAntiAlias(vg, 0);
        nvgBeginPath(vg); nvgRect(vg, x, y, width, height); nvgFillColor(vg, nvgRGB(255, 255, 255)); nvgFill(vg);
        nvgBeginPath(vg);
        for (int row = 0; row < size; ++row)
            for (int col = 0; col < size; ++col)
                if (qrcodegen_getModule(modules, col, row)) nvgRect(vg, left + (col + 4) * step, top + (row + 4) * step, step, step);
        nvgFillColor(vg, nvgRGB(0, 0, 0)); nvgFill(vg); nvgRestore(vg);
    }
};
