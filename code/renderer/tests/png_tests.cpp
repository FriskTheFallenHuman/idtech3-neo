// SPDX-License-Identifier: GPL-3.0-or-later
#include "../tr_local.h"
#include <png.h>
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
void Encode(png_uint_32 format, int width, int height, const std::vector<byte> &pixels,
            const std::vector<byte> &palette = {}) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = width;
    image.height = height;
    image.format = format;
    image.colormap_entries = static_cast<png_uint_32>(palette.size() / 4);
    png_alloc_size_t size = 0;
    Check(png_image_write_to_memory(&image, nullptr, &size, 0, pixels.data(), 0, palette.data()),
          "PNG fixture size");
    fileData.resize(size);
    Check(png_image_write_to_memory(&image, fileData.data(), &size, 0, pixels.data(), 0,
                                    palette.data()),
          "PNG fixture encoding");
    fileData.resize(size);
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
        Encode(PNG_FORMAT_RGBA, 2, 2, rgba);
        Decode(true, 2, 2);
        Check(decoded == rgba, "Exact RGBA values, alpha and top-down orientation");
        fileData.resize(fileData.size() / 2);
        Decode(false);
        fileData = {0, 1, 2, 3, 4, 5, 6, 7};
        Decode(false);
        Encode(PNG_FORMAT_GRAY, 2, 1, {0, 255});
        Decode(true, 2, 1);
        Check(decoded == std::vector<byte>({0, 0, 0, 255, 255, 255, 255, 255}),
              "Grayscale expansion");
        Encode(PNG_FORMAT_RGBA_COLORMAP, 2, 1, {0, 1}, {9, 8, 7, 0, 6, 5, 4, 255});
        Decode(true, 2, 1);
        Check(decoded == std::vector<byte>({9, 8, 7, 0, 6, 5, 4, 255}), "Palette/tRNS expansion");
        Encode(PNG_FORMAT_RGBA, 20000, 1, std::vector<byte>(80000));
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
