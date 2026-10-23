/*
===========================================================================
Hexen II port of the RTCW single-player GPL source.
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Distributed under the GNU General Public License, version 3 or later.
See COPYING.txt and the source distribution's additional terms.
===========================================================================
*/

#include "tr_local.h"
#include "tr_glprogs.h"
#include "tr_vfx.h"
#include "tr_decals.h"
#include <algorithm>
#include <cmath>
#include <vector>

#undef min
#undef max

namespace {
constexpr GLint CLAMP_TO_EDGE = 0x812F;
constexpr GLint RGBA16F = 0x881A;
constexpr GLint DEPTH_COMPONENT24 = 0x81A6;
idGLProgram particleProgram, flameProgram, compositeProgram, coronaProgram, bloomProgram;
idGLFramebuffer particleBuffer;
idGLFramebuffer bloomBuffer[2];
GLuint sceneDepth, particleColor;
GLuint bloomColor[2];
int sceneWidth, sceneHeight, particleWidth, particleHeight;
int bloomWidth, bloomHeight;
bool available, depthReady, viewHasEffects;
cvar_t *vfxScale, *vfxSoftDistance, *vfxDebug;
cvar_t *flameCoronas, *flameCoronaScale, *flameFogGlow, *flameBloom;
std::vector<const refEntity_t *> sortedParticles;

struct particleUniforms_t {
    int viewProjection, screenSize, sceneSize, projectionDepth, additive, softDistance, depthHack;
} particleUniforms;
struct flameUniforms_t {
    int inverseViewProjection, screenSize, sceneSize, viewOrigin, origin, radius, time, phase,
        color;
} flameUniforms;
struct coronaUniforms_t {
    int inverseViewProjection, viewProjection, screenSize, viewOrigin, origin, radius, color, time,
        phase, strength, fogAmount;
} coronaUniforms;
struct compositeUniforms_t {
    int screenSize, screenOffset, particleSize, sceneSize, projectionDepth, bloomScale;
} compositeUniforms;

void TextureStorage(GLuint &texture, int width, int height, GLint format, GLenum type, bool depth) {
    if (!texture) {
        qglGenTextures(1, &texture);
    }
    RB_BindVfxTexture(0, texture);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, CLAMP_TO_EDGE);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, CLAMP_TO_EDGE);
    qglTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, depth ? GL_DEPTH_COMPONENT : GL_RGBA,
                  type, nullptr);
}

void ResizeBuffers() {
    int width = backEnd.viewParms.viewportWidth;
    int height = backEnd.viewParms.viewportHeight;
    int divisor = static_cast<int>(Com_Clamp(1, 4, vfxScale->integer));
    int lowWidth = (width + divisor - 1) / divisor;
    int lowHeight = (height + divisor - 1) / divisor;
    if (sceneWidth == width && sceneHeight == height && particleWidth == lowWidth &&
        particleHeight == lowHeight) {
        return;
    }
    particleBuffer.Shutdown();
    for (auto &buffer : bloomBuffer) {
        buffer.Shutdown();
    }
    TextureStorage(sceneDepth, width, height, DEPTH_COMPONENT24, GL_UNSIGNED_INT, true);
    TextureStorage(particleColor, lowWidth, lowHeight, RGBA16F, GL_FLOAT, false);
    particleBuffer.Create(particleColor);
    bloomWidth = (lowWidth + 1) / 2;
    bloomHeight = (lowHeight + 1) / 2;
    for (int i = 0; i < 2; ++i) {
        TextureStorage(bloomColor[i], bloomWidth, bloomHeight, RGBA16F, GL_FLOAT, false);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        bloomBuffer[i].Create(bloomColor[i]);
    }
    sceneWidth = width;
    sceneHeight = height;
    particleWidth = lowWidth;
    particleHeight = lowHeight;
}

void SpriteQuad(const refEntity_t &entity) {
    vec3_t left, up;
    if (entity.reType == RT_ORIENTED_PARTICLE) {
        PerpendicularVector(left, entity.axis[0]);
        CrossProduct(entity.axis[0], left, up);
    } else {
        VectorCopy(backEnd.viewParms.or.axis[1], left);
        VectorCopy(backEnd.viewParms.or.axis[2], up);
    }
    float angle = entity.rotation * (M_PI / 180);
    float sine = sinf(angle), cosine = cosf(angle);
    vec3_t rotatedLeft, rotatedUp;
    for (int axis = 0; axis < 3; ++axis) {
        rotatedLeft[axis] = entity.radius * (left[axis] * cosine + up[axis] * sine);
        rotatedUp[axis] = entity.radius * (up[axis] * cosine - left[axis] * sine);
    }
    const float coordinates[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    qglColor4ubv(entity.shaderRGBA);
    for (int corner = 0; corner < 4; ++corner) {
        vec3_t point;
        float horizontal = corner == 0 || corner == 3 ? 1 : -1;
        float vertical = corner < 2 ? 1 : -1;
        for (int axis = 0; axis < 3; ++axis) {
            point[axis] =
                entity.origin[axis] + horizontal * rotatedLeft[axis] + vertical * rotatedUp[axis];
        }
        qglTexCoord2fv(coordinates[corner]);
        qglVertex3fv(point);
    }
}

float ViewDistance(const refEntity_t &entity) {
    vec3_t delta;
    VectorSubtract(entity.origin, backEnd.viewParms.or.origin, delta);
    return DotProduct(delta, backEnd.viewParms.or.axis[0]);
}
} // namespace

void R_InitVfx() {
    available = R_InitGLPrograms();
    if (!available) {
        ri.Error(
            ERR_FATAL,
            "Screen-space particles and decals require OpenGL 2.0 GLSL and framebuffer objects");
        return;
    }
    vfxScale = ri.Cvar_Get("r_vfxScale", "2", CVAR_ARCHIVE);
    vfxSoftDistance = ri.Cvar_Get("r_vfxSoftDistance", "6", CVAR_ARCHIVE);
    vfxDebug = ri.Cvar_Get("r_vfxDebug", "0", 0);
    flameCoronas = ri.Cvar_Get("r_flameCoronas", "1", CVAR_ARCHIVE);
    flameCoronaScale = ri.Cvar_Get("r_flameCoronaScale", "1", CVAR_ARCHIVE);
    flameFogGlow = ri.Cvar_Get("r_flameFogGlow", "0.8", CVAR_ARCHIVE);
    flameBloom = ri.Cvar_Get("r_flameBloom", "1.25", CVAR_ARCHIVE);
    particleProgram.Load("glprogs/vfx_particle.vert", "glprogs/vfx_particle.frag");
    flameProgram.Load("glprogs/screen.vert", "glprogs/vfx_flame.frag");
    coronaProgram.Load("glprogs/screen.vert", "glprogs/vfx_corona.frag");
    bloomProgram.Load("glprogs/vfx_bloom.vert", "glprogs/vfx_bloom.frag");
    compositeProgram.Load("glprogs/screen.vert", "glprogs/vfx_composite.frag");
    particleProgram.Bind();
    particleProgram.SetInt(particleProgram.Uniform("uParticle"), 0);
    particleProgram.SetInt(particleProgram.Uniform("uSceneDepth"), 1);
#define PARTICLE_UNIFORM(field, name) particleUniforms.field = particleProgram.Uniform(name)
    PARTICLE_UNIFORM(viewProjection, "uViewProjection");
    PARTICLE_UNIFORM(screenSize, "uScreenSize");
    PARTICLE_UNIFORM(sceneSize, "uSceneSize");
    PARTICLE_UNIFORM(projectionDepth, "uProjectionDepth");
    PARTICLE_UNIFORM(additive, "uAdditive");
    PARTICLE_UNIFORM(softDistance, "uSoftDistance");
    PARTICLE_UNIFORM(depthHack, "uDepthHack");
#undef PARTICLE_UNIFORM
    flameProgram.Bind();
    flameProgram.SetInt(flameProgram.Uniform("uSceneDepth"), 1);
#define FLAME_UNIFORM(field, name) flameUniforms.field = flameProgram.Uniform(name)
    FLAME_UNIFORM(inverseViewProjection, "uInverseViewProjection");
    FLAME_UNIFORM(screenSize, "uScreenSize");
    FLAME_UNIFORM(sceneSize, "uSceneSize");
    FLAME_UNIFORM(viewOrigin, "uViewOrigin");
    FLAME_UNIFORM(origin, "uOrigin");
    FLAME_UNIFORM(radius, "uRadius");
    FLAME_UNIFORM(time, "uTime");
    FLAME_UNIFORM(phase, "uPhase");
    FLAME_UNIFORM(color, "uColor");
#undef FLAME_UNIFORM
    coronaProgram.Bind();
    coronaProgram.SetInt(coronaProgram.Uniform("uSceneDepth"), 1);
#define CORONA_UNIFORM(field, name) coronaUniforms.field = coronaProgram.Uniform(name)
    CORONA_UNIFORM(inverseViewProjection, "uInverseViewProjection");
    CORONA_UNIFORM(viewProjection, "uViewProjection");
    CORONA_UNIFORM(screenSize, "uScreenSize");
    CORONA_UNIFORM(viewOrigin, "uViewOrigin");
    CORONA_UNIFORM(origin, "uOrigin");
    CORONA_UNIFORM(radius, "uRadius");
    CORONA_UNIFORM(color, "uColor");
    CORONA_UNIFORM(time, "uTime");
    CORONA_UNIFORM(phase, "uPhase");
    CORONA_UNIFORM(strength, "uStrength");
    CORONA_UNIFORM(fogAmount, "uFogAmount");
#undef CORONA_UNIFORM
    compositeProgram.Bind();
    compositeProgram.SetInt(compositeProgram.Uniform("uParticles"), 0);
    compositeProgram.SetInt(compositeProgram.Uniform("uSceneDepth"), 1);
    compositeProgram.SetInt(compositeProgram.Uniform("uBloom"), 2);
#define COMPOSITE_UNIFORM(field, name) compositeUniforms.field = compositeProgram.Uniform(name)
    COMPOSITE_UNIFORM(screenSize, "uScreenSize");
    COMPOSITE_UNIFORM(screenOffset, "uScreenOffset");
    COMPOSITE_UNIFORM(particleSize, "uParticleSize");
    COMPOSITE_UNIFORM(sceneSize, "uSceneSize");
    COMPOSITE_UNIFORM(projectionDepth, "uProjectionDepth");
    COMPOSITE_UNIFORM(bloomScale, "uBloomScale");
#undef COMPOSITE_UNIFORM
    bloomProgram.Bind();
    bloomProgram.SetInt(bloomProgram.Uniform("uImage"), 0);
    idGLProgram::Unbind();
    R_InitDecals();
    ri.Printf(PRINT_ALL,
              "Doom screen-space VFX: GLSL particles, procedural fire, depth-projected decals\n");
}

void R_ShutdownVfx() {
    if (!available) {
        return;
    }
    R_ShutdownDecals();
    particleProgram.Shutdown();
    flameProgram.Shutdown();
    coronaProgram.Shutdown();
    bloomProgram.Shutdown();
    compositeProgram.Shutdown();
    particleBuffer.Shutdown();
    for (int i = 0; i < 2; ++i) {
        bloomBuffer[i].Shutdown();
        if (bloomColor[i]) {
            qglDeleteTextures(1, &bloomColor[i]);
        }
        bloomColor[i] = 0;
    }
    if (sceneDepth) {
        qglDeleteTextures(1, &sceneDepth);
    }
    if (particleColor) {
        qglDeleteTextures(1, &particleColor);
    }
    sceneDepth = particleColor = 0;
    sceneWidth = sceneHeight = particleWidth = particleHeight = 0;
    bloomWidth = bloomHeight = 0;
    available = depthReady = viewHasEffects = false;
    sortedParticles.clear();
    R_ShutdownGLPrograms();
}

bool R_VfxAvailable() {
    return available;
}

bool R_IsScreenParticle(const refEntity_t &entity) {
    return entity.reType == RT_SPRITE || entity.reType == RT_ORIENTED_PARTICLE ||
           entity.reType == RT_SCREEN_FLAME;
}

bool RB_ScreenEntityVisible(const refEntity_t &entity) {
    if ((entity.renderfx & RF_FIRST_PERSON) && backEnd.viewParms.isPortal) {
        return false;
    }
    if ((entity.renderfx & RF_THIRD_PERSON) && !backEnd.viewParms.isPortal) {
        return false;
    }
    return entity.radius > 0;
}

void RB_BeginVfxView(bool requireDepth) {
    depthReady = true;
    viewHasEffects = false;
    if (!available || (backEnd.refdef.rdflags & RDF_NOWORLDMODEL)) {
        return;
    }
    // Capture once, after opaque geometry and before translucent geometry.
    depthReady = !requireDepth;
    for (int index = 0; index < backEnd.refdef.num_entities; ++index) {
        const refEntity_t &entity = backEnd.refdef.entities[index].e;
        if ((R_IsScreenParticle(entity) || entity.reType == RT_SCREEN_DECAL) &&
            RB_ScreenEntityVisible(entity)) {
            depthReady = false;
            viewHasEffects = true;
            break;
        }
    }
}

void RB_BindVfxTexture(int unit, unsigned int texture) {
    GL_SelectTexture(unit);
    qglBindTexture(GL_TEXTURE_2D, texture);
    glState.currenttextures[unit] = texture;
}

void RB_CaptureVfxDepth() {
    if (depthReady) {
        return;
    }
    glstate_t saved = glState;
    qglPushAttrib(GL_ALL_ATTRIB_BITS);
    ResizeBuffers();
    RB_BindVfxTexture(0, sceneDepth);
    qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, backEnd.viewParms.viewportX,
                         backEnd.viewParms.viewportY, sceneWidth, sceneHeight);
    GL_SelectTexture(saved.currenttmu);
    qglPopAttrib();
    glState = saved;
    depthReady = true;
}

void RB_VfxViewProjection(float *matrix) {
    const float *projection = backEnd.viewParms.projectionMatrix;
    const float *view = backEnd.viewParms.world.modelMatrix;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            matrix[column * 4 + row] = 0;
            for (int component = 0; component < 4; ++component) {
                matrix[column * 4 + row] +=
                    projection[component * 4 + row] * view[column * 4 + component];
            }
        }
    }
}

bool RB_VfxInverseMatrix(const float *matrix, float *inverse) {
    // Pivoted Gauss-Jordan elimination, once per view; matrices are column-major.
    double work[4][8] = {};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            work[row][column] = matrix[column * 4 + row];
        }
        work[row][row + 4] = 1;
    }
    for (int column = 0; column < 4; ++column) {
        int pivot = column;
        for (int row = column + 1; row < 4; ++row) {
            if (fabs(work[row][column]) > fabs(work[pivot][column])) {
                pivot = row;
            }
        }
        if (fabs(work[pivot][column]) < 1e-12) {
            return false;
        }
        for (int entry = 0; entry < 8; ++entry) {
            std::swap(work[column][entry], work[pivot][entry]);
        }
        double divisor = work[column][column];
        for (int entry = 0; entry < 8; ++entry) {
            work[column][entry] /= divisor;
        }
        for (int row = 0; row < 4; ++row) {
            if (row == column) {
                continue;
            }
            double factor = work[row][column];
            for (int entry = 0; entry < 8; ++entry) {
                work[row][entry] -= factor * work[column][entry];
            }
        }
    }
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            inverse[column * 4 + row] = static_cast<float>(work[row][column + 4]);
        }
    }
    return true;
}

bool RB_VfxBounds(const vec3_t origin, float radius, float height, float *rectangle) {
    float matrix[16];
    RB_VfxViewProjection(matrix);
    rectangle[0] = rectangle[1] = 1;
    rectangle[2] = rectangle[3] = -1;
    for (int corner = 0; corner < 8; ++corner) {
        vec3_t point = {origin[0] + (corner & 1 ? radius : -radius),
                        origin[1] + (corner & 2 ? radius : -radius),
                        origin[2] + (corner & 4 ? height : -radius)};
        float clip[4];
        for (int row = 0; row < 4; ++row) {
            clip[row] = matrix[row] * point[0] + matrix[4 + row] * point[1] +
                        matrix[8 + row] * point[2] + matrix[12 + row];
        }
        if (clip[3] <= .01f) {
            // An emitter crossing the near plane must use a conservative full view bound.
            rectangle[0] = rectangle[1] = -1;
            rectangle[2] = rectangle[3] = 1;
            return true;
        }
        float x = clip[0] / clip[3], y = clip[1] / clip[3];
        rectangle[0] = std::min(rectangle[0], x);
        rectangle[1] = std::min(rectangle[1], y);
        rectangle[2] = std::max(rectangle[2], x);
        rectangle[3] = std::max(rectangle[3], y);
    }
    if (rectangle[2] < -1 || rectangle[3] < -1 || rectangle[0] > 1 || rectangle[1] > 1) {
        return false;
    }
    for (int index = 0; index < 4; ++index) {
        rectangle[index] = Com_Clamp(-1, 1, rectangle[index]);
    }
    return true;
}

void RB_DrawVfxQuad(const float *rectangle) {
    qglBegin(GL_QUADS);
    qglVertex2f(rectangle[0], rectangle[1]);
    qglVertex2f(rectangle[2], rectangle[1]);
    qglVertex2f(rectangle[2], rectangle[3]);
    qglVertex2f(rectangle[0], rectangle[3]);
    qglEnd();
}

unsigned int RB_VfxDepthTexture() {
    return sceneDepth;
}
bool RB_VfxDepthReady() {
    return depthReady;
}

void RB_VfxRestoreState() {
    idGLProgram::Unbind();
    GL_SelectTexture(0);
}

static void RB_RenderVfxPass(bool flamePass) {
    if (!viewHasEffects) {
        return;
    }
    RB_CaptureVfxDepth();
    std::vector<const refEntity_t *> &entities = sortedParticles;
    entities.clear();
    entities.reserve(backEnd.refdef.num_entities);
    for (int index = 0; index < backEnd.refdef.num_entities; ++index) {
        const refEntity_t &entity = backEnd.refdef.entities[index].e;
        if ((entity.reType == RT_SCREEN_FLAME) != flamePass) {
            continue;
        }
        if (!R_IsScreenParticle(entity) || !RB_ScreenEntityVisible(entity) ||
            ViewDistance(entity) < -entity.radius * 8) {
            continue;
        }
        entities.push_back(&entity);
    }
    if (entities.empty()) {
        return;
    }
    // Premultiplied smoke and blood require back-to-front ordering, including fire.
    std::sort(entities.begin(), entities.end(), [](const refEntity_t *a, const refEntity_t *b) {
        float first = ViewDistance(*a), second = ViewDistance(*b);
        return first != second ? first > second : a < b;
    });
    glstate_t saved = glState;
    qglPushAttrib(GL_ALL_ATTRIB_BITS);
    qglDisable(GL_FOG);
    qglDisable(GL_CLIP_PLANE0);
    GL_Cull(CT_TWO_SIDED);
    GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
    qglDepthRange(0, 1);
    particleBuffer.Bind();
    qglViewport(0, 0, particleWidth, particleHeight);
    qglScissor(0, 0, particleWidth, particleHeight);
    qglClearColor(0, 0, 0, 0);
    qglClear(GL_COLOR_BUFFER_BIT);
    RB_BindVfxTexture(1, sceneDepth);
    float viewProjection[16], inverse[16];
    RB_VfxViewProjection(viewProjection);
    if (!RB_VfxInverseMatrix(viewProjection, inverse)) {
        idGLFramebuffer::BindDefault();
        GL_SelectTexture(saved.currenttmu);
        qglPopAttrib();
        glState = saved;
        return;
    }
    particleProgram.Bind();
    particleProgram.SetMatrix(particleUniforms.viewProjection, viewProjection);
    particleProgram.SetVec2(particleUniforms.screenSize, particleWidth, particleHeight);
    particleProgram.SetVec2(particleUniforms.sceneSize, sceneWidth, sceneHeight);
    particleProgram.SetVec2(particleUniforms.projectionDepth,
                            backEnd.viewParms.projectionMatrix[10],
                            backEnd.viewParms.projectionMatrix[14]);
    particleProgram.SetFloat(particleUniforms.softDistance,
                             Com_Clamp(.1f, 64, vfxSoftDistance->value));
    flameProgram.Bind();
    flameProgram.SetMatrix(flameUniforms.inverseViewProjection, inverse);
    flameProgram.SetVec2(flameUniforms.screenSize, particleWidth, particleHeight);
    flameProgram.SetVec2(flameUniforms.sceneSize, sceneWidth, sceneHeight);
    flameProgram.SetVec3(flameUniforms.viewOrigin, backEnd.viewParms.or.origin);
    bool drawingFlame = true;
    bool spriteBatch = false;
    GLuint spriteTexture = 0;
    int spriteAdditive = -1, spriteDepthHack = -1;
    int flames = 0;
    for (const refEntity_t *entity : entities) {
        bool flame = entity->reType == RT_SCREEN_FLAME;
        if (flame && spriteBatch) {
            qglEnd();
            spriteBatch = false;
        }
        if (flame != drawingFlame) {
            if (flame) {
                flameProgram.Bind();
            } else {
                particleProgram.Bind();
            }
            drawingFlame = flame;
        }
        if (flame) {
            float rectangle[4];
            if (!RB_VfxBounds(entity->origin, entity->radius * 2, entity->radius * 8, rectangle)) {
                continue;
            }
            flameProgram.SetVec3(flameUniforms.origin, entity->origin);
            flameProgram.SetFloat(flameUniforms.radius, entity->radius);
            flameProgram.SetFloat(flameUniforms.time, entity->effectTime);
            flameProgram.SetFloat(flameUniforms.phase, entity->effectSeed);
            float color[4];
            for (int index = 0; index < 4; ++index) {
                color[index] = entity->shaderRGBA[index] / 255.0f;
            }
            flameProgram.SetVec4(flameUniforms.color, color);
            RB_DrawVfxQuad(rectangle);
            ++flames;
        } else {
            shader_t *shader = R_GetShaderByHandle(entity->customShader);
            for (int stageIndex = 0; stageIndex < shader->numUnfoggedPasses; ++stageIndex) {
                const shaderStage_t *stage = shader->stages[stageIndex];
                const textureBundle_t &bundle = stage->bundle[0];
                int imageIndex = 0;
                if (bundle.numImageAnimations > 1) {
                    imageIndex = static_cast<int>(
                                     std::max(0.0f, backEnd.refdef.floatTime - entity->shaderTime) *
                                     bundle.imageAnimationSpeed) %
                                 bundle.numImageAnimations;
                }
                image_t *image = bundle.image[imageIndex];
                if (!image) {
                    continue;
                }
                image->frameUsed = tr.frameCount;
                int additive = (stage->stateBits & GLS_DSTBLEND_BITS) == GLS_DSTBLEND_ONE;
                int depthHack = (entity->renderfx & RF_DEPTHHACK) != 0;
                if (!spriteBatch || spriteTexture != image->texnum || spriteAdditive != additive ||
                    spriteDepthHack != depthHack) {
                    if (spriteBatch) {
                        qglEnd();
                    }
                    RB_BindVfxTexture(0, image->texnum);
                    particleProgram.SetInt(particleUniforms.additive, additive);
                    particleProgram.SetInt(particleUniforms.depthHack, depthHack);
                    qglBegin(GL_QUADS);
                    spriteBatch = true;
                    spriteTexture = image->texnum;
                    spriteAdditive = additive;
                    spriteDepthHack = depthHack;
                }
                SpriteQuad(*entity);
            }
        }
    }
    if (spriteBatch) {
        qglEnd();
    }
    const float fullView[4] = {-1, -1, 1, 1};
    const float bloomStrength = flamePass ? Com_Clamp(0, 4, flameBloom->value) : 0;
    if (bloomStrength > 0) {
        // Extract HDR flame cores and blur at quarter viewport resolution.
        // Smoke, blood and the environment never feed this emissive bloom.
        GL_State(GLS_DEPTHTEST_DISABLE);
        qglViewport(0, 0, bloomWidth, bloomHeight);
        qglScissor(0, 0, bloomWidth, bloomHeight);
        bloomProgram.Bind();
        for (int pass = 0; pass < 2; ++pass) {
            bloomBuffer[pass].Bind();
            RB_BindVfxTexture(0, pass == 0 ? particleColor : bloomColor[0]);
            bloomProgram.SetVec2(bloomProgram.Uniform("uDirection"),
                                 pass == 0 ? 2.5f / particleWidth : 0,
                                 pass == 1 ? 1.25f / bloomHeight : 0);
            bloomProgram.SetFloat(bloomProgram.Uniform("uThreshold"), pass == 0 ? .75f : 0);
            RB_DrawVfxQuad(fullView);
        }
        GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
    }
    idGLFramebuffer::BindDefault();
    qglViewport(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, sceneWidth, sceneHeight);
    qglScissor(backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, sceneWidth, sceneHeight);
    compositeProgram.Bind();
    RB_BindVfxTexture(0, particleColor);
    if (bloomStrength > 0) {
        qglActiveTextureARB(GL_TEXTURE0_ARB + 2);
        qglBindTexture(GL_TEXTURE_2D, bloomColor[1]);
        qglActiveTextureARB(GL_TEXTURE0_ARB + glState.currenttmu);
    }
    compositeProgram.SetFloat(compositeUniforms.bloomScale, bloomStrength);
    compositeProgram.SetVec2(compositeUniforms.screenSize, sceneWidth, sceneHeight);
    compositeProgram.SetVec2(compositeUniforms.screenOffset, backEnd.viewParms.viewportX,
                             backEnd.viewParms.viewportY);
    compositeProgram.SetVec2(compositeUniforms.particleSize, particleWidth, particleHeight);
    compositeProgram.SetVec2(compositeUniforms.sceneSize, sceneWidth, sceneHeight);
    compositeProgram.SetVec2(compositeUniforms.projectionDepth,
                             backEnd.viewParms.projectionMatrix[10],
                             backEnd.viewParms.projectionMatrix[14]);
    RB_DrawVfxQuad(fullView);
    idGLProgram::Unbind();
    GL_SelectTexture(saved.currenttmu);
    qglPopAttrib();
    glState = saved;
    if (vfxDebug->integer && backEnd.viewParms.frameCount % 120 == 0) {
        ri.Printf(PRINT_ALL, "Screen VFX: %u particles (%d flames), %dx%d -> %dx%d bloom=%.2f\n",
                  static_cast<unsigned>(entities.size()), flames, particleWidth, particleHeight,
                  sceneWidth, sceneHeight, bloomStrength);
    }
}

void RB_RenderVfx() {
    RB_RenderVfxPass(false);
}
void RB_RenderFlames() {
    RB_RenderVfxPass(true);
}

void RB_DrawFlameCoronas() {
    if (!available || !flameCoronas->integer || !viewHasEffects || !RB_VfxDepthReady() ||
        (backEnd.refdef.rdflags & RDF_NOWORLDMODEL)) {
        return;
    }
    float viewProjection[16], inverse[16];
    RB_VfxViewProjection(viewProjection);
    if (!RB_VfxInverseMatrix(viewProjection, inverse)) {
        return;
    }
    glstate_t saved = glState;
    qglPushAttrib(GL_ALL_ATTRIB_BITS);
    qglDisable(GL_FOG);
    qglDisable(GL_CLIP_PLANE0);
    GL_Cull(CT_TWO_SIDED);
    GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE);
    RB_BindVfxTexture(1, sceneDepth);
    coronaProgram.Bind();
    coronaProgram.SetMatrix(coronaUniforms.inverseViewProjection, inverse);
    coronaProgram.SetMatrix(coronaUniforms.viewProjection, viewProjection);
    coronaProgram.SetVec2(coronaUniforms.screenSize, sceneWidth, sceneHeight);
    coronaProgram.SetVec2(coronaProgram.Uniform("uScreenOffset"), backEnd.viewParms.viewportX,
                          backEnd.viewParms.viewportY);
    coronaProgram.SetVec3(coronaUniforms.viewOrigin, backEnd.viewParms.or.origin);
    coronaProgram.SetFloat(coronaUniforms.strength, Com_Clamp(0, 2, flameFogGlow->value));
    int coronas = 0;
    for (int i = 0; i < backEnd.refdef.num_entities; ++i) {
        const refEntity_t &entity = backEnd.refdef.entities[i].e;
        if (entity.reType != RT_SCREEN_FLAME || !RB_ScreenEntityVisible(entity)) {
            continue;
        }
        vec3_t center, towardView;
        VectorCopy(entity.origin, center);
        // The visible flame lies above its solid cup, slightly in front of wall-mounted fixtures.
        center[2] += entity.radius * 4;
        VectorSubtract(backEnd.viewParms.or.origin, center, towardView);
        VectorNormalize(towardView);
        VectorMA(center, entity.radius * .5f, towardView, center);
        const float radius = entity.radius * Com_Clamp(.25f, 2, flameCoronaScale->value);
        float rectangle[4];
        if (!RB_VfxBounds(center, radius * 10, radius * 10, rectangle)) {
            continue;
        }
        float color[4];
        for (int component = 0; component < 4; ++component) {
            color[component] = entity.shaderRGBA[component] / 255.0f;
        }
        coronaProgram.SetVec3(coronaUniforms.origin, center);
        coronaProgram.SetFloat(coronaUniforms.radius, radius);
        coronaProgram.SetFloat(coronaUniforms.time, entity.effectTime);
        coronaProgram.SetFloat(coronaUniforms.phase, entity.effectSeed);
        coronaProgram.SetVec4(coronaUniforms.color, color);
        coronaProgram.SetFloat(coronaUniforms.fogAmount, 0.0f);
        RB_DrawVfxQuad(rectangle);
        ++coronas;
    }
    idGLProgram::Unbind();
    GL_SelectTexture(saved.currenttmu);
    qglPopAttrib();
    glState = saved;
    if (vfxDebug->integer && backEnd.viewParms.frameCount % 120 == 0) {
        ri.Printf(PRINT_ALL, "Flame coronas: %d, composited over world fog\n", coronas);
    }
}
