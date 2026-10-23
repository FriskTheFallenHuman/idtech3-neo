// SPDX-License-Identifier: GPL-3.0-or-later
// Decode through vendored stb_image into the renderer's reusable image allocation.
#include "../renderer/tr_local.h"
#include <limits.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
    int w = 0, h = 0, comp = 0;
    // Check the dimensions before anything is allocated so oversized
    // images are rejected without touching the renderer's image memory.
    if (!stbi_info_from_memory(file, length, &w, &h, &comp) || w <= 0 || h <= 0) {
        ri.Printf(PRINT_WARNING, "Could not decode PNG %s: not a valid PNG\n", name);
        ri.FS_FreeFile(file);
        return;
    }
    const uint64_t bytes = uint64_t(w) * h * 4;
    if (w > 16384 || h > 16384 || bytes > INT_MAX) {
        ri.Printf(PRINT_WARNING, "PNG %s has unsupported dimensions %d x %d\n", name, w, h);
        ri.FS_FreeFile(file);
        return;
    }
    stbi_uc *rgba = stbi_load_from_memory(file, length, &w, &h, &comp, 4);
    ri.FS_FreeFile(file);
    if (!rgba) {
        ri.Printf(PRINT_WARNING, "Could not decode PNG %s: %s\n", name,
                  stbi_failure_reason());
        return;
    }
    byte *pixels = R_AllocateImagePixels(static_cast<int>(bytes));
    memcpy(pixels, rgba, static_cast<size_t>(bytes));
    free(rgba);
    *pic = pixels;
    *width = w;
    *height = h;
}
