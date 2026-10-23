// Engine bridge for Raven's generic effect system, adapted to Doom / RTCW.
#include "common_headers.h"
#include "FxScheduler.h"

vmCvar_t fx_debug, fx_freeze, fx_expensivePhysics;

int FX_IRand(int low, int high) {
    if (high <= low) {
        return low;
    }
    // RTCW random() includes 1.0, so float scaling can return high + 1.
    // Scale 30 random bits in integer arithmetic; wide intermediates also handle
    // the complete signed-int range without overflowing high - low + 1.
    unsigned long long bits = (static_cast<unsigned long long>(rand() & 0x7fff) << 15) |
                              static_cast<unsigned long long>(rand() & 0x7fff);
    unsigned long long range =
        static_cast<unsigned long long>(static_cast<long long>(high) - low + 1);
    return static_cast<int>(static_cast<long long>(low) +
                            static_cast<long long>((bits * range) >> 30));
}
float FX_FRand(float low, float high) {
    return low + random() * (high - low);
}

void SFxHelper::Init() {
    mTime = mFrameTime = mEmitterKey = 0;
    mFloatFrameTime = 0;
}
void SFxHelper::AdjustTime(int frameTime) {
    mFrameTime = fx_freeze.integer || cg_paused.integer ? 0 : (int)Com_Clamp(0, 100, frameTime);
    mFloatFrameTime = mFrameTime * .001f;
    mTime += mFrameTime;
}
void SFxHelper::Print(const char *format, ...) {
    char message[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    CG_Printf("%s", message);
}
int SFxHelper::OpenFile(const char *path, fileHandle_t *file, int mode) {
    return trap_FS_FOpenFile(path, file, (fsMode_t)mode);
}
int SFxHelper::ReadFile(void *data, int length, fileHandle_t file) {
    trap_FS_Read(data, length, file);
    return length;
}
void SFxHelper::CloseFile(fileHandle_t file) {
    trap_FS_FCloseFile(file);
}
void SFxHelper::PlaySound(const vec3_t origin, int entity, int channel, sfxHandle_t sound) {
    trap_S_StartSound((float *)origin, entity, channel, sound);
}
void SFxHelper::PlayLocalSound(sfxHandle_t sound, int channel) {
    trap_S_StartLocalSound(sound, channel);
}
int SFxHelper::RegisterSound(const char *sound) {
    return trap_S_RegisterSound(sound);
}
int SFxHelper::RegisterShader(const char *shader) {
    return trap_R_RegisterShader(shader);
}
int SFxHelper::RegisterModel(const char *model) {
    return trap_R_RegisterModel(model);
}
void SFxHelper::Trace(trace_t *trace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int skip,
                      int mask) {
    CG_Trace(trace, start, mins, maxs, end, skip, mask);
}
void SFxHelper::AddFxToScene(refEntity_t *entity) {
    trap_R_AddRefEntityToScene(entity);
}
void SFxHelper::AddLightToScene(vec3_t origin, float radius, float red, float green, float blue) {
    trap_R_AddLightToScene(origin, radius, red, green, blue, 0);
}
void SFxHelper::AddPolyToScene(int shader, int count, polyVert_t *vertices) {
    trap_R_AddPolyToScene(shader, count, vertices);
}
void SFxHelper::CameraShake(vec3_t origin, float intensity, int radius, int duration) {
    CG_StartShakeCamera(intensity, duration, origin, (float)radius);
}
int SFxHelper::GetEntityAttachment(const centity_t &entity, int model, int tag, vec3_t origin,
                                   vec3_t axis[3]) {
    if (!entity.currentValid) {
        return 0;
    }
    VectorCopy(entity.lerpOrigin, origin);
    AnglesToAxis(entity.lerpAngles, axis);
    // Named MD3 attachment tags are optional for sprite-based Doom weapons.
    if (tag > 0 && entity.refEnt.hModel) {
        orientation_t local;
        if (trap_R_LerpTag(&local, &entity.refEnt, "tag_flash", 0) >= 0) {
            for (int i = 0; i < 3; ++i) {
                VectorMA(origin, local.origin[i], entity.refEnt.axis[i], origin);
            }
            MatrixMultiply(local.axis, (vec3_t *)entity.refEnt.axis, axis);
        }
    }
    return 1;
}

static void FX_Vertex(polyVert_t &vertex, const vec3_t origin, float s, float t,
                      const refEntity_t &entity) {
    VectorCopy(origin, vertex.xyz);
    vertex.st[0] = s;
    vertex.st[1] = t;
    memcpy(vertex.modulate, entity.shaderRGBA, 4);
}
void FX_DrawRibbon(const refEntity_t &entity, const vec3_t start, const vec3_t end, float width,
                   float endWidth) {
    vec3_t direction, view, side, point;
    VectorSubtract(end, start, direction);
    VectorSubtract(cg.refdef.vieworg, start, view);
    CrossProduct(direction, view, side);
    if (VectorNormalize(side) < .001f) {
        VectorCopy(cg.refdef.viewaxis[1], side);
    }
    polyVert_t vertices[4];
    VectorMA(start, width, side, point);
    FX_Vertex(vertices[0], point, 0, 0, entity);
    VectorMA(start, -width, side, point);
    FX_Vertex(vertices[1], point, 0, 1, entity);
    VectorMA(end, -endWidth, side, point);
    FX_Vertex(vertices[2], point, 1, 1, entity);
    VectorMA(end, endWidth, side, point);
    FX_Vertex(vertices[3], point, 1, 0, entity);
    trap_R_AddPolyToScene(entity.customShader, 4, vertices);
}
void FX_DrawQuad(const refEntity_t &entity, const vec3_t normal) {
    vec3_t right, up, point;
    MakeNormalVectors(normal, right, up);
    vec3_t rotated;
    RotatePointAroundVector(rotated, normal, right, entity.rotation);
    VectorCopy(rotated, right);
    CrossProduct(normal, right, up);
    polyVert_t vertices[4];
    const float corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (int i = 0; i < 4; ++i) {
        VectorMA(entity.origin, corners[i][0] * entity.radius, right, point);
        VectorMA(point, corners[i][1] * entity.radius, up, point);
        FX_Vertex(vertices[i], point, (corners[i][0] + 1) * .5f, (corners[i][1] + 1) * .5f, entity);
    }
    trap_R_AddPolyToScene(entity.customShader, 4, vertices);
}
void FX_DrawCylinder(const refEntity_t &entity) {
    vec3_t direction, right, up, radial, point;
    VectorSubtract(entity.oldorigin, entity.origin, direction);
    if (VectorNormalize(direction) < .001f) {
        VectorCopy(entity.axis[0], direction);
    }
    MakeNormalVectors(direction, right, up);
    const int sides = 16;
    for (int i = 0; i < sides; ++i) {
        polyVert_t vertices[4];
        for (int j = 0; j < 4; ++j) {
            float angle = (i + (j == 1 || j == 2)) * 2 * M_PI / sides;
            VectorScale(right, cosf(angle), radial);
            VectorMA(radial, sinf(angle), up, radial);
            bool far = j >= 2;
            VectorMA(far ? entity.oldorigin : entity.origin, far ? entity.backlerp : entity.radius,
                     radial, point);
            FX_Vertex(vertices[j], point, (i + (j == 1 || j == 2)) / (float)sides, far ? 1 : 0,
                      entity);
        }
        trap_R_AddPolyToScene(entity.customShader, 4, vertices);
    }
}
