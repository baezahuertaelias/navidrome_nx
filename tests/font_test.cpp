#define STB_TRUETYPE_IMPLEMENTATION
#include <nanovg/stb_truetype.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
std::vector<unsigned char> read(const char* path) {
    std::ifstream file(path, std::ios::binary); assert(file);
    return {std::istreambuf_iterator<char>(file), {}};
}
int main() {
    auto text = read("resources/inter/NavidromeMusic.ttf");
    auto icons = read("resources/material/MaterialIcons-Regular.ttf");
    stbtt_fontinfo regular{}, material{};
    assert(stbtt_InitFont(&regular, text.data(), 0)); assert(stbtt_InitFont(&material, icons.data(), 0));
    for (int code : {0xe034, 0xe037, 0xe044, 0xe045, 0xe047, 0xe5c4, 0xe5d2, 0xe5d5,
                    0xe7fd, 0xe019, 0xe03d, 0xe87d, 0xe244, 0xe245, 0xe627, 0xe628, 0xe8b8, 0xe88a, 0xe5cb, 0xe5cc, 0xe8ef, 0xe8f0, 0xe05f, 0xe8d5}) {
        assert(stbtt_FindGlyphIndex(&regular, code) == 0);
        assert(stbtt_FindGlyphIndex(&material, code) != 0);
    }
    auto boldBytes = read("resources/inter/NavidromeLyrics-Bold.ttf");
    stbtt_fontinfo bold{}; assert(stbtt_InitFont(&bold, boldBytes.data(), 0));
    for (int code : {0x41, 0x7a, 0xf1, 0xe9, 0xbf}) assert(stbtt_FindGlyphIndex(&bold, code) != 0);
    for (int code : {0x41, 0x7a, 0xf1, 0xe9, 0xbf}) assert(stbtt_FindGlyphIndex(&regular, code) != 0);
}
