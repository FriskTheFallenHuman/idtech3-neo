// SPDX-License-Identifier: GPL-3.0-or-later
// Decode through vendored libpng into the renderer's reusable image allocation.
#include "../renderer/tr_local.h"
#include <limits.h>
#include <png.h>

void R_LoadPNG(const char *name, byte **pic, int *width, int *height) {
    *pic = nullptr;
    *width = *height = 0;
    byte *file = nullptr;
    const int length = ri.FS_ReadFile(name, reinterpret_cast<void **>(&file));
    if (!file || length <= 0) {
        if (file) {
            ri.FS_FreeFile(file);
        }
        return;
    }
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, file, static_cast<size_t>(length))) {
        const uint64_t bytes = uint64_t(image.width) * image.height * 4;
        if (image.width && image.height && image.width <= 16384 && image.height <= 16384 &&
            bytes <= INT_MAX) {
            image.format = PNG_FORMAT_RGBA;
            byte *rgba = R_AllocateImagePixels(static_cast<int>(bytes));
            if (png_image_finish_read(&image, nullptr, rgba, 0, nullptr)) {
                *pic = rgba;
                *width = static_cast<int>(image.width);
                *height = static_cast<int>(image.height);
            }
        } else {
            ri.Printf(PRINT_WARNING, "PNG %s has unsupported dimensions %u x %u\n", name,
                      image.width, image.height);
        }
    }
    if (!*pic) {
        ri.Printf(PRINT_WARNING, "Could not decode PNG %s: %s\n", name, image.message);
    }
    png_image_free(&image);
    ri.FS_FreeFile(file);
}
