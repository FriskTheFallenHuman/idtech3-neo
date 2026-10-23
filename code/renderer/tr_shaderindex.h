// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../game/q_shared.h"

struct shaderStringPointer_t {
    char *pStr;
    shaderStringPointer_t *next;
};

// Index definition names only. Allocation must have the same lifetime as text.
// Keeping traversal here lets the renderer and regression test exercise exactly
// the same brace handling without linking a graphics context into the test.
template <typename Hash, typename Allocate>
void R_BuildShaderTextIndex(char *text, shaderStringPointer_t *buckets, int count, Hash hash,
                            Allocate allocate, bool clear = true) {
    if (clear) {
        memset(buckets, 0, count * sizeof(*buckets));
    }
    char *cursor = text;
    while (cursor) {
        char *start = cursor;
        const char *token = COM_ParseExt(&cursor, qtrue);
        if (!*token) {
            break;
        }
        if (!Q_stricmp(token, "{")) {
            // The parser needs to consume the opening brace itself.
            cursor = start;
            SkipBracedSection(&cursor);
            continue;
        }
        const unsigned int bucket = hash(token) % static_cast<unsigned int>(count);
        auto *node = &buckets[bucket];
        if (node->pStr) {
            node = allocate();
            node->next = buckets[bucket].next;
            buckets[bucket].next = node;
        }
        node->pStr = start;
    }
}
