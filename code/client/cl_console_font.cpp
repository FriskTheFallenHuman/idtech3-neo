// SPDX-License-Identifier: GPL-3.0-or-later
#include "client.h"
#include "console_font.h"
#include "console_layout.h"
#include <algorithm>

namespace {
fontInfo_t consoleFont;
cvar_t *fontPath, *fontSize, *fontScale;
int rasterSize = 0, cellWidth = SMALLCHAR_WIDTH, lineHeight = SMALLCHAR_HEIGHT, ascent = 0;
char loadedPath[MAX_QPATH];
bool ready = false;

void FontInfoCommand() {
    Com_Printf("Console font: %s truetype=%d pixels=%d cell=%dx%d columns=%d video=%dx%d\n",
               loadedPath, ready ? 1 : 0, rasterSize, cellWidth, lineHeight,
               doomconsole::Columns(cls.glconfig.vidWidth, cellWidth), cls.glconfig.vidWidth,
               cls.glconfig.vidHeight);
}
} // namespace

void Con_FontInit() {
    fontPath = Cvar_Get("con_font", "fonts/SourceCodePro-Regular.ttf", CVAR_ARCHIVE);
    fontSize = Cvar_Get("con_fontSize", "16", CVAR_ARCHIVE);
    fontScale = Cvar_Get("con_scale", "1", CVAR_ARCHIVE);
    Cmd_AddCommand("con_fontinfo", FontInfoCommand);
    ready = false;
    rasterSize = 0;
    loadedPath[0] = 0;
    Com_Memset(&consoleFont, 0, sizeof(consoleFont));
    Con_FontUpdate();
}

void Con_FontUpdate() {
    if (!fontPath || !re.RegisterFont || cls.glconfig.vidHeight < 1) {
        return;
    }
    const int pixels =
        doomconsole::PixelSize(cls.glconfig.vidHeight, fontSize->value, fontScale->value);
    if (pixels == rasterSize && !Q_stricmp(loadedPath, fontPath->string)) {
        return;
    }
    rasterSize = pixels;
    Q_strncpyz(loadedPath, fontPath->string, sizeof(loadedPath));
    re.RegisterFont(loadedPath, pixels, &consoleFont);
    ready = consoleFont.glyphs['M'].glyph && consoleFont.glyphs['M'].xSkip > 0;
    if (!ready) {
        cellWidth = SMALLCHAR_WIDTH;
        lineHeight = SMALLCHAR_HEIGHT;
        Com_Printf("Console TrueType font unavailable; using the bitmap fallback\n");
        return;
    }
    cellWidth = 1;
    ascent = 0;
    int descent = 0;
    for (int c = 32; c < GLYPHS_PER_FONT; ++c) {
        const auto &glyph = consoleFont.glyphs[c];
        cellWidth = (std::max)(cellWidth, glyph.xSkip);
        ascent = (std::max)(ascent, glyph.top);
        descent = (std::max)(descent, glyph.height - glyph.top);
    }
    lineHeight = (std::max)(pixels, ascent + descent) + (std::max)(2, pixels / 6);
    FontInfoCommand();
}

int Con_FontWidth() {
    return cellWidth;
}
int Con_FontHeight() {
    return lineHeight;
}

void Con_FontDrawChar(int x, int y, int character) {
    const int c = character & 255;
    if (!ready) {
        SCR_DrawSmallChar(x, y, c);
        return;
    }
    if (c == ' ' || y + lineHeight <= 0 || x + cellWidth <= 0 || x >= cls.glconfig.vidWidth ||
        y >= cls.glconfig.vidHeight) {
        return;
    }
    if (c == 10 || c == 11) {
        const float height = c == 10 ? 2.0f : static_cast<float>(lineHeight);
        re.DrawStretchPic(static_cast<float>(x), static_cast<float>(y + lineHeight) - height,
                          static_cast<float>(cellWidth), height, 0, 0, 1, 1, cls.whiteShader);
        return;
    }
    const auto &glyph = consoleFont.glyphs[c];
    if (!glyph.imageWidth || !glyph.imageHeight) {
        return;
    }
    // Baselines and fixed advances are shared by output, prompt, edit text and cursor.
    re.DrawStretchPic(static_cast<float>(x), static_cast<float>(y + ascent - glyph.top),
                      static_cast<float>(glyph.imageWidth), static_cast<float>(glyph.imageHeight),
                      glyph.s, glyph.t, glyph.s2, glyph.t2, glyph.glyph);
}

void Con_FontDrawString(int x, int y, const char *text, const float *color) {
    re.SetColor(color);
    for (const char *cursor = text; *cursor; ++cursor) {
        if (Q_IsColorString(cursor)) {
            vec4_t tint;
            Com_Memcpy(tint, g_color_table[ColorIndex(cursor[1])], sizeof(tint));
            tint[3] = color[3];
            re.SetColor(tint);
            ++cursor;
            continue;
        }
        Con_FontDrawChar(x, y, static_cast<unsigned char>(*cursor));
        x += cellWidth;
    }
    re.SetColor(nullptr);
}
