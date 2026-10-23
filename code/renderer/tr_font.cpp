/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// Runtime TrueType atlases. Font identity includes both filename and pixel size;
// the old point-size-only DAT cache could silently return another typeface.
#include "tr_local.h"
#include "../ft2/freetype.h"
#include <algorithm>
#include <climits>

namespace {
constexpr int MaxFonts = 48;
constexpr int AtlasSize = 1024;
FT_Library fontLibrary = nullptr;
fontInfo_t registeredFonts[MaxFonts];
char registeredNames[MaxFonts][MAX_QPATH];
int registeredSizes[MaxFonts];
int registeredCount = 0;

unsigned FontHash(const char *name) {
    unsigned hash = 2166136261u;
    for (; *name; ++name) {
        hash = (hash ^ static_cast<unsigned char>(*name)) * 16777619u;
    }
    return hash;
}
} // namespace

void RE_RegisterFont(const char *fontName, int pixelSize, fontInfo_t *font) {
    if (!font) {
        return;
    }
    Com_Memset(font, 0, sizeof(*font));
    if (!fontLibrary || !fontName || !*fontName || strlen(fontName) >= MAX_QPATH || pixelSize < 6 ||
        pixelSize > 64) {
        ri.Printf(PRINT_WARNING, "Invalid TrueType font request: %s (%d pixels)\n",
                  fontName ? fontName : "<null>", pixelSize);
        return;
    }
    for (int i = 0; i < registeredCount; ++i) {
        if (registeredSizes[i] == pixelSize && !Q_stricmp(registeredNames[i], fontName)) {
            *font = registeredFonts[i];
            return;
        }
    }
    if (registeredCount == MaxFonts) {
        ri.Printf(PRINT_WARNING, "TrueType font cache is full; restart video to change fonts\n");
        return;
    }
    void *fileData = nullptr;
    const int length = ri.FS_ReadFile(fontName, &fileData);
    if (length <= 0 || length > 16 * 1024 * 1024) {
        if (fileData) {
            ri.FS_FreeFile(fileData);
        }
        ri.Printf(PRINT_WARNING, "Cannot read TrueType font: %s\n", fontName);
        return;
    }
    FT_Face face = nullptr;
    const auto error =
        FT_New_Memory_Face(fontLibrary, static_cast<FT_Byte *>(fileData), length, 0, &face);
    if (error || FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixelSize))) {
        if (face) {
            FT_Done_Face(face);
        }
        ri.FS_FreeFile(fileData);
        ri.Printf(PRINT_WARNING, "Cannot initialize TrueType face: %s (%d pixels)\n", fontName,
                  pixelSize);
        return;
    }
    // Engine temp storage avoids depending on C++ destructors across renderer error paths.
    auto *atlas = static_cast<byte *>(ri.Hunk_AllocateTempMemory(AtlasSize * AtlasSize * 4));
    Com_Memset(atlas, 0, AtlasSize * AtlasSize * 4);
    int x = 2, y = 2, rowHeight = 0;
    bool valid = true;
    for (int c = 0; c < GLYPHS_PER_FONT; ++c) {
        if (c < 32) {
            continue;
        }
        if (FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_RENDER)) {
            valid = false;
            break;
        }
        const auto &bitmap = face->glyph->bitmap;
        const int width = bitmap.width, height = bitmap.rows;
        if (width < 0 || height < 0 || width > 512 || height > 512 ||
            (bitmap.pixel_mode != ft_pixel_mode_grays && bitmap.pixel_mode != ft_pixel_mode_mono)) {
            valid = false;
            break;
        }
        if (x + width + 2 > AtlasSize) {
            x = 2;
            y += rowHeight + 4;
            rowHeight = 0;
        }
        if (y + height + 2 > AtlasSize || (width && height && !bitmap.buffer)) {
            valid = false;
            break;
        }
        const int sourcePitch = bitmap.pitch < 0 ? -bitmap.pitch : bitmap.pitch;
        const int minimumPitch = bitmap.pixel_mode == ft_pixel_mode_mono ? (width + 7) / 8 : width;
        if (sourcePitch < minimumPitch) {
            valid = false;
            break;
        }
        auto &glyph = font->glyphs[c];
        glyph.height = glyph.imageHeight = height;
        glyph.pitch = glyph.imageWidth = width;
        glyph.top = face->glyph->bitmap_top;
        glyph.bottom = glyph.top - height;
        glyph.xSkip = static_cast<int>((face->glyph->advance.x + 32) / 64);
        glyph.s = static_cast<float>(x) / AtlasSize;
        glyph.t = static_cast<float>(y) / AtlasSize;
        glyph.s2 = static_cast<float>(x + width) / AtlasSize;
        glyph.t2 = static_cast<float>(y + height) / AtlasSize;
        for (int row = 0; row < height; ++row) {
            const int sourceRow = bitmap.pitch < 0 ? height - 1 - row : row;
            const auto *source = bitmap.buffer + sourceRow * sourcePitch;
            for (int column = 0; column < width; ++column) {
                auto *pixel = atlas + ((y + row) * AtlasSize + x + column) * 4;
                pixel[0] = pixel[1] = pixel[2] = 255;
                pixel[3] = bitmap.pixel_mode == ft_pixel_mode_mono
                               ? ((source[column / 8] & (0x80 >> (column & 7))) ? 255 : 0)
                               : source[column];
            }
        }
        x += width + 4;
        rowHeight = (std::max)(rowHeight, height);
    }
    FT_Done_Face(face);
    ri.FS_FreeFile(fileData);
    if (!valid) {
        ri.Hunk_FreeTempMemory(atlas);
        Com_Memset(font, 0, sizeof(*font));
        ri.Printf(PRINT_WARNING, "Invalid or oversized TrueType glyph atlas: %s\n", fontName);
        return;
    }
    char imageName[32];
    Com_sprintf(imageName, sizeof(imageName), "fonts/rt%08x_%d", FontHash(fontName), pixelSize);
    R_SyncRenderThread();
    auto *image = R_CreateImage(imageName, atlas, AtlasSize, AtlasSize, qfalse, qfalse, GL_CLAMP);
    ri.Hunk_FreeTempMemory(atlas);
    const auto shader = RE_RegisterShaderFromImage(imageName, LIGHTMAP_2D, image, qfalse);
    for (auto &glyph : font->glyphs) {
        glyph.glyph = shader;
        Q_strncpyz(glyph.shaderName, imageName, sizeof(glyph.shaderName));
    }
    font->glyphScale = 48.0f / pixelSize;
    Q_strncpyz(font->name, imageName, sizeof(font->name));
    registeredFonts[registeredCount] = *font;
    Q_strncpyz(registeredNames[registeredCount], fontName, MAX_QPATH);
    registeredSizes[registeredCount++] = pixelSize;
    ri.Printf(PRINT_ALL, "TrueType font: %s pixels=%d atlas=%dx%d\n", fontName, pixelSize,
              AtlasSize, AtlasSize);
}

void R_InitFreeType() {
    registeredCount = 0;
    if (!fontLibrary && FT_Init_FreeType(&fontLibrary)) {
        ri.Printf(PRINT_WARNING, "Cannot initialize FreeType\n");
    }
}

void R_DoneFreeType() {
    registeredCount = 0;
    if (fontLibrary) {
        FT_Done_FreeType(fontLibrary);
        fontLibrary = nullptr;
    }
}
