// SPDX-License-Identifier: GPL-3.0-or-later
#include "tr_local.h"
#include "tr_glprogs.h"
#include "tr_editor.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
struct TextureSlot {
    uint64_t handle = 0;
    GLuint object = 0;
};
std::array<TextureSlot, 4096> textures;
uint64_t nextHandle = 1, previewTexture = 0;
GLuint previewDepth = 0;
int previewWidth = 0, previewHeight = 0;
idGLFramebuffer previewTarget;

GLuint textureObject(uint64_t handle) {
    for (const auto &slot : textures) {
        if (slot.handle == handle && handle) {
            return slot.object;
        }
    }
    return 0;
}

// Restore both real GL state and the engine's state cache. The editor is drawn
// between BeginFrame/EndFrame and must not poison the next game frame.
struct RenderState {
    glstate_t cached = glState;
    unsigned int program = idGLProgram::Current();
    GLint matrixMode = GL_MODELVIEW;
    GLint unpackAlignment = 4;
    RenderState() {
        qglGetIntegerv(GL_MATRIX_MODE, &matrixMode);
        qglGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
        qglPushAttrib(GL_ALL_ATTRIB_BITS);
        qglMatrixMode(GL_PROJECTION);
        qglPushMatrix();
        qglMatrixMode(GL_MODELVIEW);
        qglPushMatrix();
        idGLProgram::Unbind();
        if (qglActiveTextureARB) {
            GL_SelectTexture(1);
            qglDisable(GL_TEXTURE_2D);
            GL_SelectTexture(0);
        }
        qglEnable(GL_TEXTURE_2D);
        qglDisable(GL_FOG);
        qglDisable(GL_CULL_FACE);
        qglDisable(GL_CLIP_PLANE0);
        qglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        qglEnable(GL_SCISSOR_TEST);
        qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    ~RenderState() {
        idGLFramebuffer::BindDefault();
        qglMatrixMode(GL_MODELVIEW);
        qglPopMatrix();
        qglMatrixMode(GL_PROJECTION);
        qglPopMatrix();
        qglMatrixMode(matrixMode);
        qglPopAttrib();
        qglPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
        idGLProgram::Restore(program);
        glState = cached;
    }
};
uint64_t createTexture(int width, int height, const uint32_t *pixels) {
    if (!tr.registered || width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
        width > glConfig.maxTextureSize || height > glConfig.maxTextureSize) {
        return 0;
    }
    auto free = std::find_if(textures.begin(), textures.end(),
                             [](const TextureSlot &slot) { return !slot.handle; });
    if (free == textures.end()) {
        return 0;
    }
    R_SyncRenderThread();
    RenderState state;
    GLuint object = 0;
    qglGenTextures(1, &object);
    if (!object) {
        return 0;
    }
    qglBindTexture(GL_TEXTURE_2D, object);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    if (qglGetError() != GL_NO_ERROR) {
        qglDeleteTextures(1, &object);
        return 0;
    }
    *free = {nextHandle++, object};
    return free->handle;
}
void destroyTexture(uint64_t handle) {
    for (auto &slot : textures) {
        if (slot.handle == handle && handle) {
            R_SyncRenderThread();
            qglDeleteTextures(1, &slot.object);
            slot = {};
            return;
        }
    }
}
bool validFloat(float value) {
    return std::isfinite(value);
}
bool drawUI(const EditorUIDraw *draw) {
    constexpr uint32_t limit = 4 * 1024 * 1024;
    if (!tr.registered || !draw || !draw->vertices || !draw->indices || !draw->commands ||
        draw->vertexCount > limit || draw->indexCount > limit || draw->commandCount > 65536 ||
        !validFloat(draw->width) || !validFloat(draw->height) || draw->width < 1 ||
        draw->height < 1 || draw->width > 16384 || draw->height > 16384) {
        return false;
    }
    for (uint32_t i = 0; i < draw->vertexCount; ++i) {
        const auto &vertex = draw->vertices[i];
        if (!validFloat(vertex.position[0]) || !validFloat(vertex.position[1]) ||
            !validFloat(vertex.uv[0]) || !validFloat(vertex.uv[1])) {
            return false;
        }
    }
    for (uint32_t i = 0; i < draw->indexCount; ++i) {
        if (draw->indices[i] >= draw->vertexCount) {
            return false;
        }
    }
    for (uint32_t i = 0; i < draw->commandCount; ++i) {
        const auto &command = draw->commands[i];
        if (command.firstIndex > draw->indexCount ||
            command.indexCount > draw->indexCount - command.firstIndex || command.indexCount % 3 ||
            !std::all_of(command.clip, command.clip + 4, validFloat)) {
            return false;
        }
        if (!textureObject(command.texture)) {
            ri.Printf(PRINT_WARNING, "Editor UI command %u references an expired texture handle\n",
                      i);
            return false;
        }
    }
    R_SyncRenderThread();
    RenderState state;
    const int editorWidth = static_cast<int>(draw->width);
    const int editorHeight = static_cast<int>(draw->height);
    qglViewport(0, 0, editorWidth, editorHeight);
    qglScissor(0, 0, editorWidth, editorHeight);
    qglClearColor(.055f, .065f, .08f, 1);
    qglClear(GL_COLOR_BUFFER_BIT);
    qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity();
    qglOrtho(0, draw->width, draw->height, 0, -1, 1);
    qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity();
    qglDisable(GL_DEPTH_TEST);
    qglDepthMask(GL_FALSE);
    qglDisable(GL_ALPHA_TEST);
    qglEnable(GL_BLEND);
    qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const float sx = editorWidth / draw->width, sy = editorHeight / draw->height;
    for (uint32_t i = 0; i < draw->commandCount; ++i) {
        const auto &command = draw->commands[i];
        const int left =
            static_cast<int>(std::clamp(command.clip[0] * sx, 0.f, float(editorWidth)));
        const int top =
            static_cast<int>(std::clamp(command.clip[1] * sy, 0.f, float(editorHeight)));
        const int right =
            static_cast<int>(std::clamp(command.clip[2] * sx, 0.f, float(editorWidth)));
        const int bottom =
            static_cast<int>(std::clamp(command.clip[3] * sy, 0.f, float(editorHeight)));
        if (right <= left || bottom <= top) {
            continue;
        }
        qglScissor(left, editorHeight - bottom, right - left, bottom - top);
        qglBindTexture(GL_TEXTURE_2D, textureObject(command.texture));
        qglBegin(GL_TRIANGLES);
        for (uint32_t j = 0; j < command.indexCount; ++j) {
            const auto &vertex = draw->vertices[draw->indices[command.firstIndex + j]];
            qglColor4ubv(reinterpret_cast<const byte *>(&vertex.color));
            qglTexCoord2fv(vertex.uv);
            qglVertex2fv(vertex.position);
        }
        qglEnd();
    }
    return true;
}
uint64_t renderPreview(const EditorPreviewScene *scene) {
    if (!tr.registered || !scene || (scene->vertexCount && !scene->vertices) ||
        (scene->batchCount && !scene->batches) || scene->width <= 0 || scene->height <= 0 ||
        scene->width > 4096 || scene->height > 4096 || scene->vertexCount > 4 * 1024 * 1024 ||
        scene->batchCount > 65536) {
        return 0;
    }
    for (uint32_t i = 0; i < scene->batchCount; ++i) {
        const auto &batch = scene->batches[i];
        if (batch.firstVertex > scene->vertexCount ||
            batch.vertexCount > scene->vertexCount - batch.firstVertex || batch.vertexCount % 3 ||
            !textureObject(batch.texture)) {
            return 0;
        }
    }
    for (const auto *vector : {scene->eye, scene->forward, scene->right, scene->up}) {
        if (!std::all_of(vector, vector + 3, validFloat)) {
            return 0;
        }
    }
    for (uint32_t i = 0; i < scene->vertexCount; ++i) {
        const auto &vertex = scene->vertices[i];
        if (!std::all_of(vertex.position, vertex.position + 3, validFloat) ||
            !std::all_of(vertex.uv, vertex.uv + 2, validFloat)) {
            return 0;
        }
    }
    R_SyncRenderThread();
    if (previewWidth != scene->width || previewHeight != scene->height || !previewTexture) {
        previewTarget.Shutdown();
        destroyTexture(previewTexture);
        if (previewDepth) {
            qglDeleteTextures(1, &previewDepth);
            previewDepth = 0;
        }
        previewTexture = createTexture(scene->width, scene->height, nullptr);
        if (!previewTexture) {
            return 0;
        }
        RenderState state;
        qglGenTextures(1, &previewDepth);
        qglBindTexture(GL_TEXTURE_2D, previewDepth);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        qglTexImage2D(GL_TEXTURE_2D, 0, 0x81a6 /* DEPTH_COMPONENT24 */, scene->width, scene->height,
                      0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        if (!previewTarget.CreateColorDepth(textureObject(previewTexture), previewDepth)) {
            return 0;
        }
        previewWidth = scene->width;
        previewHeight = scene->height;
    }
    RenderState state;
    previewTarget.Bind();
    qglViewport(0, 0, scene->width, scene->height);
    qglScissor(0, 0, scene->width, scene->height);
    qglDepthMask(GL_TRUE);
    qglClearColor(.12f, .15f, .19f, 1);
    qglClearDepth(1);
    qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    qglEnable(GL_DEPTH_TEST);
    qglDepthFunc(GL_LEQUAL);
    qglEnable(GL_ALPHA_TEST);
    qglAlphaFunc(GL_GREATER, .3f);
    qglDisable(GL_BLEND);
    qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity();
    qglFrustum(-1, 1, -double(scene->height) / scene->width, double(scene->height) / scene->width,
               1, 65536);
    float matrix[16] = {scene->right[0],
                        scene->up[0],
                        -scene->forward[0],
                        0,
                        scene->right[1],
                        scene->up[1],
                        -scene->forward[1],
                        0,
                        scene->right[2],
                        scene->up[2],
                        -scene->forward[2],
                        0,
                        0,
                        0,
                        0,
                        1};
    qglMatrixMode(GL_MODELVIEW);
    qglLoadMatrixf(matrix);
    qglTranslatef(-scene->eye[0], -scene->eye[1], -scene->eye[2]);
    for (uint32_t i = 0; i < scene->batchCount; ++i) {
        const auto &batch = scene->batches[i];
        qglBindTexture(GL_TEXTURE_2D, textureObject(batch.texture));
        qglBegin(GL_TRIANGLES);
        for (uint32_t j = 0; j < batch.vertexCount; ++j) {
            const auto &vertex = scene->vertices[batch.firstVertex + j];
            qglColor4ubv(reinterpret_cast<const byte *>(&vertex.color));
            qglTexCoord2fv(vertex.uv);
            qglVertex3fv(vertex.position);
        }
        qglEnd();
    }
    return previewTexture;
}
} // namespace

EditorRenderAPI R_EditorRenderAPI() {
    return {createTexture, destroyTexture, renderPreview, drawUI};
}
void R_ShutdownEditorRenderer() {
    previewTarget.Shutdown();
    if (previewDepth) {
        qglDeleteTextures(1, &previewDepth);
        previewDepth = 0;
    }
    for (auto &slot : textures) {
        if (slot.object) {
            qglDeleteTextures(1, &slot.object);
        }
        slot = {};
    }
    previewTexture = 0;
    previewWidth = previewHeight = 0;
}
