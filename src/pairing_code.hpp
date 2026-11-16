#pragma once
#include <string>
#include "../vendor/qrcodegen/qrcodegen.h"

inline constexpr size_t PairingQrBytes = qrcodegen_BUFFER_LEN_FOR_VERSION(6);
inline std::string pairingUrl(const std::string& url, const std::string& code) {
    return url.empty() || code.empty() ? "" : url + "/#" + code;
}
inline bool encodePairingQr(const std::string& value, uint8_t (&modules)[PairingQrBytes]) {
    uint8_t temporary[PairingQrBytes];
    return !value.empty() && qrcodegen_encodeText(value.c_str(), temporary, modules,
        qrcodegen_Ecc_MEDIUM, 1, 6, qrcodegen_Mask_AUTO, true);
}
