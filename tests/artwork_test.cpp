#define STB_IMAGE_IMPLEMENTATION
#include "artwork.hpp"
#include <cassert>
#include <iostream>

int main() {
    int width, height;
    assert(!coverDimensions(nullptr, 0, width, height));
    const unsigned char bad[] = "not an image";
    assert(!coverDimensions(bad, sizeof(bad), width, height));
    // A tiny binary PPM exercises the same metadata validator as network artwork.
    const unsigned char image[] = "P6\n1 1\n255\n\xff\xff\xff";
    assert(coverDimensions(image, sizeof(image) - 1, width, height));
    assert(width == 1 && height == 1);
    assert(!coverDimensions(image, 512 * 1024 + 1, width, height));
    const unsigned char large[] = "P6\n2048 2048\n255\n";
    assert(!coverDimensions(large, sizeof(large) - 1, width, height));
    int channels;
    auto decoded = stbi_load_from_memory(image, sizeof(image) - 1, &width, &height, &channels, 4);
    assert(decoded && width == 1 && height == 1);
    stbi_image_free(decoded);
    std::cout << "Artwork checks passed\n";
}
