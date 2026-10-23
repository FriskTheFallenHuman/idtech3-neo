// SPDX-License-Identifier: GPL-3.0-or-later
#include "../tr_local.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

refimport_t ri{};
namespace {
std::vector<byte> fileData, decoded;
int fileReads, fileFrees, allocations;
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
int Read(const char *, void **buffer) {
    ++fileReads;
    *buffer = fileData.empty() ? nullptr : fileData.data();
    return static_cast<int>(fileData.size());
}
void Free(void *) {
    ++fileFrees;
}
void QDECL Print(int, const char *, ...) {
}
void Be32(std::vector<byte> &b, uint32_t n) {
    b.push_back(byte(n >> 24));
    b.push_back(byte(n >> 16));
    b.push_back(byte(n >> 8));
    b.push_back(byte(n));
}
uint32_t Crc32(const byte *data, size_t size) {
    uint32_t crc = ~0u;
    while (size--) {
        crc ^= *data++;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
        }
    }
    return ~crc;
}
void Chunk(std::vector<byte> &png, const char *type, const std::vector<byte> &data) {
    Be32(png, uint32_t(data.size()));
    const size_t start = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), data.begin(), data.end());
    Be32(png, Crc32(png.data() + start, png.size() - start));
}
// A zlib stream of stored deflate blocks, enough for fixtures without a compressor.
std::vector<byte> ZlibStore(const std::vector<byte> &raw) {
    std::vector<byte> z = {0x78, 0x01};
    size_t start = 0;
    while (start < raw.size()) {
        const size_t n = std::min(size_t(65535), raw.size() - start);
        z.push_back(start + n == raw.size() ? 1 : 0);
        z.push_back(byte(n));
        z.push_back(byte(n >> 8));
        const uint16_t inverse = uint16_t(~uint16_t(n));
        z.push_back(byte(inverse));
        z.push_back(byte(inverse >> 8));
        z.insert(z.end(), raw.begin() + start, raw.begin() + start + n);
        start += n;
    }
    uint32_t a = 1, b = 0;
    for (byte c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    Be32(z, (b << 16) | a);
    return z;
}
void Encode(int colorType, int channels, int width, int height,
            const std::vector<byte> &pixels, const std::vector<byte> &palette = {},
            const std::vector<byte> &transparency = {}) {
    std::vector<byte> png = {137, 80, 78, 71, 13, 10, 26, 10}, ihdr;
    Be32(ihdr, uint32_t(width));
    Be32(ihdr, uint32_t(height));
    ihdr.push_back(8); // bit depth
    ihdr.push_back(byte(colorType));
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // no interlace
    Chunk(png, "IHDR", ihdr);
    if (!palette.empty()) {
        Chunk(png, "PLTE", palette);
    }
    if (!transparency.empty()) {
        Chunk(png, "tRNS", transparency);
    }
    std::vector<byte> raw;
    raw.reserve(size_t(height) * (1 + size_t(width) * channels));
    for (int row = 0; row < height; ++row) {
        raw.push_back(0); // no filter
        raw.insert(raw.end(), pixels.begin() + size_t(row) * width * channels,
                   pixels.begin() + size_t(row + 1) * width * channels);
    }
    Chunk(png, "IDAT", ZlibStore(raw));
    Chunk(png, "IEND", {});
    fileData = std::move(png);
}
void Decode(bool expected, int expectedWidth = 0, int expectedHeight = 0) {
    byte *pixels = reinterpret_cast<byte *>(1);
    int width = -1, height = -1;
    R_LoadPNG("fixture.png", &pixels, &width, &height);
    Check(expected ? pixels && width == expectedWidth && height == expectedHeight
                   : !pixels && width == 0 && height == 0,
          "PNG decoder result/dimensions");
}
} // namespace
byte *R_AllocateImagePixels(int size) {
    ++allocations;
    decoded.resize(size);
    return decoded.data();
}
int main() {
    try {
        ri.FS_ReadFile = Read;
        ri.FS_FreeFile = Free;
        ri.Printf = Print;
        const std::vector<byte> rgba = {255, 0, 0, 255, 0, 255, 0, 0, 0, 0, 255, 128, 9, 8, 7, 255};
        Encode(6, 4, 2, 2, rgba);
        Decode(true, 2, 2);
        Check(decoded == rgba, "Exact RGBA values, alpha and top-down orientation");
        fileData.resize(fileData.size() / 2);
        Decode(false);
        fileData = {0, 1, 2, 3, 4, 5, 6, 7};
        Decode(false);
        Encode(0, 1, 2, 1, {0, 255});
        Decode(true, 2, 1);
        Check(decoded == std::vector<byte>({0, 0, 0, 255, 255, 255, 255, 255}),
              "Grayscale expansion");
        Encode(3, 1, 2, 1, {0, 1}, {9, 8, 7, 6, 5, 4}, {0});
        Decode(true, 2, 1);
        Check(decoded == std::vector<byte>({9, 8, 7, 0, 6, 5, 4, 255}), "Palette/tRNS expansion");
        Encode(6, 4, 20000, 1, std::vector<byte>(80000));
        const int before = allocations;
        Decode(false);
        Check(allocations == before, "Reject oversized images before renderer allocation");
        Check(fileReads == fileFrees, "Release every filesystem buffer including decode failures");
        puts("Vendored PNG decoder checks passed: RGBA, alpha, orientation, grayscale, "
             "palette/tRNS, truncation, invalid signature, size bounds and buffer lifetime.");
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
