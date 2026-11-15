#pragma once
#include <nanovg/stb_image.h>

inline bool coverDimensions(const unsigned char* data, int size, int& width, int& height) {
    int channels;
    return data && size > 0 && size <= 512 * 1024
        && stbi_info_from_memory(data, size, &width, &height, &channels)
        && width > 0 && height > 0 && width <= 1024 && height <= 1024;
}
