// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

// Borrowed POD data. The renderer owns GPU resources; editor handles are opaque
// identifiers, never GL objects. No ImGui, STL or allocator ABI crosses modules.
struct EditorUIVertex {
    float position[2], uv[2];
    uint32_t color;
};
struct EditorUICommand {
    uint32_t firstIndex, indexCount;
    uint64_t texture;
    float clip[4];
};
struct EditorUIDraw {
    const EditorUIVertex *vertices;
    const uint32_t *indices;
    const EditorUICommand *commands;
    uint32_t vertexCount, indexCount, commandCount;
    float width, height;
};
struct EditorSceneVertex {
    float position[3], uv[2];
    uint32_t color;
};
struct EditorSceneBatch {
    uint64_t texture;
    uint32_t firstVertex, vertexCount;
};
struct EditorPreviewScene {
    const EditorSceneVertex *vertices;
    const EditorSceneBatch *batches;
    uint32_t vertexCount, batchCount;
    int width, height;
    float eye[3], forward[3], right[3], up[3];
};
struct EditorRenderAPI {
    uint64_t (*createTexture)(int width, int height, const uint32_t *rgba);
    void (*destroyTexture)(uint64_t texture);
    uint64_t (*renderPreview)(const EditorPreviewScene *scene);
    bool (*drawUI)(const EditorUIDraw *draw);
};
