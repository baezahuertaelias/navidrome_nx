#include "pairing_code.hpp"
#include <cassert>
#include <fstream>
int main() {
    uint8_t modules[PairingQrBytes];
    assert(!encodePairingQr("", modules));
    assert(pairingUrl("", "code").empty());
    assert(pairingUrl("http://192.168.1.91:8080", "").empty());
    const auto url = pairingUrl("http://192.168.1.91:8080", "0123456789abcdef0123456789abcdef");
    assert(url == "http://192.168.1.91:8080/#0123456789abcdef0123456789abcdef");
    assert(encodePairingQr(url, modules));
    const int size = qrcodegen_getSize(modules), pixels = (size + 8) * 6;
    std::ofstream image("/tmp/pairing-qr.pgm"); image << "P2\n" << pixels << ' ' << pixels << "\n255\n";
    for (int y = 0; y < pixels; ++y)
        for (int x = 0; x < pixels; ++x)
            image << (qrcodegen_getModule(modules, x / 6 - 4, y / 6 - 4) ? 0 : 255) << '\n';
}
