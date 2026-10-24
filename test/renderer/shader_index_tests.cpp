// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the renderer's actual definition traversal with forced hash collisions.
#include "renderer/tr_shaderindex.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

shaderStringPointer_t shaderChecksumLookup[4096];
namespace {
std::vector<void *> allocations;
shaderStringPointer_t *Allocate() {
    auto *memory =
        static_cast<shaderStringPointer_t *>(std::calloc(1, sizeof(shaderStringPointer_t)));
    if (!memory) {
        throw std::bad_alloc();
    }
    allocations.push_back(memory);
    return memory;
}
} // namespace
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}

int main() {
    try {
        std::string text;
        constexpr int Count = 12000;
        for (int i = 0; i < Count; ++i) {
            text += "test/sprite" + std::to_string(i) +
                    "\n{\ncull none\n{\nmap test/pixel\nrgbGen vertex\n}\n}\n";
        }
        R_BuildShaderTextIndex(
            text.data(), shaderChecksumLookup, 4096, [](const char *) { return 1u; }, Allocate);
        int count = 0;
        for (const auto &bucket : shaderChecksumLookup) {
            for (const auto *node = &bucket; node && node->pStr; node = node->next) {
                char *cursor = node->pStr;
                const char *name = COM_ParseExt(&cursor, qtrue);
                if (strncmp(name, "test/sprite", 11)) {
                    throw std::runtime_error("Shader body was indexed as a shader name");
                }
                ++count;
            }
        }
        if (count != Count) {
            throw std::runtime_error("Shader definitions were lost or duplicated");
        }
        char malformed[] = "broken/shader\n{\n{\nmap missing\n}\n";
        char isolated[] = "textures/check/floor\n{\ncull none\n{\nmap floor.png\n}\n}\n";
        R_BuildShaderTextIndex(
            malformed, shaderChecksumLookup, 4096, [](const char *) { return 1u; }, Allocate);
        R_BuildShaderTextIndex(
            isolated, shaderChecksumLookup, 4096, [](const char *) { return 1u; }, Allocate, false);
        bool found = false;
        for (auto *node = &shaderChecksumLookup[1]; node && node->pStr; node = node->next) {
            char *cursor = node->pStr;
            if (!Q_stricmp(COM_ParseExt(&cursor, qtrue), "textures/check/floor")) {
                found = !Q_stricmp(COM_ParseExt(&cursor, qtrue), "{");
            }
        }
        if (!found) {
            throw std::runtime_error("Malformed shader hid another file's definitions");
        }
        for (void *memory : allocations) {
            std::free(memory);
        }
        std::puts("Shader index handles 12,000 nested definitions without indexing body tokens.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
