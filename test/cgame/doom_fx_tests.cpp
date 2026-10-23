// Exercise the actual effect parser, scheduler, collision and render adapters.
#include "cgame/fx/common_headers.h"
#include "cgame/fx/FxScheduler.h"
#include "cgame/fx/FxFlame.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cstdarg>

cg_t cg = {};
cgs_t cgs = {};
centity_t cg_entities[MAX_GENTITIES] = {};
vmCvar_t cg_paused = {};
vmCvar_t cg_markTime = {};
static FILE *files[32] = {};
static int sprites, polys, decals, lights, traces, failures;
static float lastFlameTime;
static vec3_t lastFlameOrigin;
static int flameDraws;
static float maximumSpriteRadius;
static refEntity_t lastDecal;
extern int activeFx;
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #test);                         \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)
void QDECL Com_Printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
}
void QDECL Com_Error(int, const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::exit(1);
}
void QDECL CG_Printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char message[2048];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    std::printf("%s", message);
    // Unknown fields and missing files must make the test fail.
    if (strstr(message, "Unknown") || strstr(message, "failed") || strstr(message, "INVALID") ||
        strstr(message, "Invalid"))
 {
        ++failures;
    }
}
int trap_FS_FOpenFile(const char *name, fileHandle_t *handle, fsMode_t) {
    *handle = 0;
    const bool oversized = !strcmp(name, "effects/__test_oversized.efx");
    const bool empty = !strcmp(name, "effects/__test_empty.efx");
    FILE *file = oversized || empty ? std::tmpfile()
                                    : std::fopen((std::string("base/") + name).c_str(), "rb");
    if (!file) {
        return -1;
    }
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    if (oversized) {
        size = 1024 * 1024 + 1;
    }
    rewind(file);
    for (int i = 1; i < 32; ++i) {
        if (!files[i]) {
            files[i] = file;
            *handle = i;
            return size;
        }
    }
    std::fclose(file);
    return -1;
}
void trap_FS_Read(void *data, int count, fileHandle_t handle) {
    CHECK(files[handle]);
    CHECK(fread(data, 1, count, files[handle]) == (size_t)count);
}
void trap_FS_FCloseFile(fileHandle_t handle) {
    CHECK(files[handle]);
    fclose(files[handle]);
    files[handle] = nullptr;
}
qhandle_t trap_R_RegisterShader(const char *name) {
    if (strncmp(name, "doom/fx/", 8) != 0) {
        std::fprintf(stderr, "Unexpected shader: <%s>\n", name);
    }
    CHECK(strncmp(name, "doom/fx/", 8) == 0);
    return 1;
}
qhandle_t trap_R_RegisterModel(const char *) {
    return 1;
}
sfxHandle_t trap_S_RegisterSound(const char *) {
    return 1;
}
void trap_S_StartSound(vec3_t, int, int, sfxHandle_t) {
}
void trap_S_StartLocalSound(sfxHandle_t, int) {
}
void CG_StartShakeCamera(float, int, vec3_t, float) {
}
int trap_R_LerpTag(orientation_t *, const refEntity_t *, const char *, int) {
    return -1;
}
static void Finite(const vec3_t point) {
    for (int i = 0; i < 3; ++i) {
        CHECK(std::isfinite(point[i]));
    }
}
void trap_R_AddRefEntityToScene(const refEntity_t *entity) {
    Finite(entity->origin);
    CHECK(std::isfinite(entity->radius));
    if (entity->radius > maximumSpriteRadius) {
        ma
ximumSpriteRadius = entity->radius;
    }
    ++sprites;
    if (entity->reType == RT_SCREEN_FLAME) {
        CHECK(entity->radius > 0);
        CHECK(std::isfinite(entity->effectTime) && entity->effectTime >= 0);
        CHECK(std::isfinite(entity->effectSeed));
        VectorCopy(entity->origin, lastFlameOrigin);
        lastFlameTime = entity->effectTime;
        ++flameDraws;
    }
    if (entity->reType == RT_SCREEN_DECAL) {
        CHECK(entity->radius > 0 && entity->effectThickness >= 1 && entity->effectThickness <= 6);
        for (int i = 0; i < 3; ++i) {
            Finite(entity->axis[i]);
            CHECK(std::fabs(VectorLength(entity->axis[i]) - 1) < .001f);
        }
        CHECK(std::fabs(DotProduct(entity->axis[0], entity->axis[1])) < .001f);
        lastDecal = *entity;
        ++decals;
    }
}
void trap_R_AddPolyToScene(qhandle_t, int count, const polyVert_t *vertices) {
    for (int i = 0; i < count; ++i) {
        Finite(vertices[i].xyz);
    }
    ++polys;
}
void trap_R_AddLightToScene(const vec3_t origin, float radius, float, float, float, int) {
    Finite(origin);
    CHECK(std::isfinite(radius));
    ++lights;
}
int CG_PointContents(const vec3_t point, int) {
    return point[2] < 0 ? CONTENTS_SOLID : 0;
}
void CG_Trace(trace_t *result, const vec3_t start, const vec3_t, const vec3_t, const vec3_t end,
              int, int) {
    ++traces;
    memset(result, 0, sizeof(*result));
    result->fraction = 1;
    result->entityNum = ENTITYNUM_WORLD;
    VectorCopy(end, result->endpos);
    VectorSet(result->plane.normal, 0, 0, 1);
    if (start[2] >= 0 && end[2] < 0) {
        result->fraction = start[2] / (start[2] - end[2]);
        for (int i = 0; i < 3; ++i) {
            result->endpos[i] = start[i] + result->fraction * (end[i] - start[i]);
        }
    }
}
void QDECL CG_Error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::exit(1);
}
static voi
d Frame(int time) {
    int previous = cg.time;
    cg.time = time;
    cg.frametime = time - previous;
    theFxHelper.AdjustTime(cg.frametime);
    theFxScheduler.AddScheduledEffects(false);
    CG_AddMarks();
}
bool CG_DoomFxAllowDecal(const vec3_t, const vec3_t) {
    return true;
}

int main() {
    // Regression: inclusive floating random() used to produce one-past-end media indices.
    CMediaHandles single, multiple, empty;
    single.AddHandle(37);
    multiple.AddHandle(11);
    multiple.AddHandle(22);
    multiple.AddHandle(33);
    CHECK(empty.GetHandle() == 0);
    bool minimumSeen = false, maximumSeen = false, widePositive = false, wideNegative = false;
    for (int sample = 0; sample < 200000; ++sample) {
        CHECK(single.GetHandle() == 37);
        int handle = multiple.GetHandle();
        CHECK(handle == 11 || handle == 22 || handle == 33);
        int value = FX_IRand(-3, 5);
        CHECK(value >= -3 && value <= 5);
        minimumSeen |= value == -3;
        maximumSeen |= value == 5;
        int wide = FX_IRand(-2147483647 - 1, 2147483647);
        widePositive |= wide > 0;
        wideNegative |= wide < 0;
    }
    CHECK(minimumSeen && maximumSeen && widePositive && wideNegative && FX_IRand(9, 4) == 9);
    VectorSet(cg.refdef.vieworg, -200, 0, 50);
    AxisClear(cg.refdef.viewaxis);
    fx_expensivePhysics.integer = 1;
    cg_markTime.integer = 45000;
    CG_InitMarkPolys();
    FX_Init();
    CHECK(theFxScheduler.RegisterEffect(std::string(1000, 'x').c_str()) == 0);
    CHECK(theFxScheduler.RegisterEffect("../outside") == 0);
    CHECK(theFxScheduler.RegisterEffect("__test_missing") == 0);
    CHECK(theFxScheduler.RegisterEffect("__test_empty") == 0);
    CHECK(theFxScheduler.RegisterEffect("__test_oversized") == 0);
    for (FILE *file : files) {
        CHECK(file == nullptr);
    }
    failures = 0; // The invalid registrations above deliberately report diagnostics.
    std::vector<int> effects;
    for (const auto &entry : std::file
system::directory_iterator("base/effects/doom")) {
        if (entry.path().extension() != ".efx") {
            continue;
        }
        std::string name = "doom/" + entry.path().stem().string();
        int handle = theFxScheduler.RegisterEffect(name.c_str());
        CHECK(handle > 0);
        CHECK(theFxScheduler.RegisterEffect(name.c_str()) == handle);
        effects.push_back(handle);
    }
    CHECK(effects.size() == 72 && failures == 0);
    vec3_t origin = {0, 0, 60}, forward = {1, 0, 0};
    Frame(16);
    for (int handle : effects) {
        theFxScheduler.PlayEffect(handle, origin, forward);
    }
    for (int time = 32; time < 4000; time += 16) {
        Frame(time);
    }
    std::printf("FX counters: sprites=%d polys=%d lights=%d traces=%d decals=%d active=%d\n",
                sprites, polys, lights, traces, decals, activeFx);
    CHECK(sprites > 100 && lights > 0 && traces > 0 && decals > 0);
    CHECK(activeFx == 0 && theFxScheduler.NumScheduledFx() == 0 && failures == 0);
    maximumSpriteRadius = 0;
    theFxScheduler.PlayEffect(theFxScheduler.RegisterEffect("doom/bfg_blast"), origin, forward);
    for (int time = 4000; time < 5000; time += 16) {
        Frame(time);
    }
    std::printf("BFG blast maximum sprite radius: %.1f\n", maximumSpriteRadius);
    CHECK(maximumSpriteRadius > 75);
    refEntity_t ribbon{};
    ribbon.customShader = 1;
    memset(ribbon.shaderRGBA, 255, sizeof(ribbon.shaderRGBA));
    vec3_t end = {80, 10, 60};
    FX_DrawRibbon(ribbon, origin, end, 2, 0.5f);
    CHECK(polys == 1);
    FX_Stop();
    CHECK(activeFx == 0);
    // Moving emitters reuse one value-only GPU source and preserve freeze timing.
    Frame(14020);
    vec3_t color = {1, 1, 1}, velocity = {0, 0, 0};
    theFxHelper.mEmitterKey = 101;
    FX_AddFlame(origin, velocity, 4, color, 250, 1);
    Frame(14040);
    CHECK(activeFx == 1 && flameDraws > 0);
    float initialTime = lastFlameTime;
    for (int time = 14060; time <= 14400; time += 20) {
     
   origin[0] += 2;
        FX_AddFlame(origin, velocity, 4, color, 250, 1);
        Frame(time);
        CHECK(activeFx == 1);
    }
    CHECK(lastFlameTime > initialTime);
    fx_freeze.integer = 1;
    float frozenTime = lastFlameTime;
    Frame(14420);
    Frame(14440);
    CHECK(lastFlameTime == frozenTime && activeFx == 1);
    fx_freeze.integer = 0;
    Frame(14460);
    theFxHelper.mEmitterKey = 102;
    FX_AddFlame(origin, velocity, 4, color, 250, 1);
    CHECK(activeFx == 2);
    FX_Stop();
    CHECK(activeFx == 0);
    for (int key = 1; key <= 129; ++key) {
        theFxHelper.mEmitterKey = key;
        FX_AddFlame(origin, velocity, 4, color, 250, 1);
    }
    CHECK(activeFx == 128);
    FX_Stop();
    theFxHelper.mEmitterKey = 0;
    fx_freeze.integer = 1;
    theFxScheduler.PlayEffect(effects.front(), origin, forward);
    CHECK(activeFx == 0);
    fx_freeze.integer = 0;
    FX_Free();
    CHECK(activeFx == 0);
    // Reload must re-register templates and leave no stale scene or file handles.
    FX_Init();
    CHECK(theFxScheduler.RegisterEffect("doom/pistol_flesh") > 0);
    FX_Free();
    for (FILE *file : files) {
        CHECK(file == nullptr);
    }
    // Exercise real mark lifetime, projection basis, alpha fading and recycling.
    CG_InitMarkPolys();
    cg.time = 20000;
    int before = decals;
    vec3_t normal = {0, 0, 1};
    CG_ImpactMark(1, origin, normal, 37, .6f, .2f, .1f, .5f, qtrue, 16, qfalse, 1000);
    CG_AddMarks();
    CHECK(decals == before + 1 && lastDecal.shaderRGBA[3] == 127);
    cg.time = 20750;
    CG_AddMarks();
    CHECK(lastDecal.shaderRGBA[3] == 63);
    before = decals;
    cg.time = 21000;
    CG_AddMarks();
    CHECK(decals == before);
    CG_ImpactMark(1, origin, normal, 0, 1, 1, 1, 1, qtrue, 16, qtrue, 1000);
    CHECK(decals == before + 1);
    CG_AddMarks();
    CHECK(decals == before + 1);
    CG_ImpactMark(1, origin, normal, 0, 1, 1, 1, 1, qtrue, 0, qfalse, 1000);
    CHECK(decals == before + 1);
    for (int 
i = 0; i < MAX_MARK_POLYS + 8; ++i) {
        CG_ImpactMark(1, origin, normal, 0, 1, 1, 1, 1, qtrue, 16, qfalse, 1000);
    }
    before = decals;
    CG_AddMarks();
    CHECK(decals > before && decals - before <= MAX_MARK_POLYS);
    CG_InitMarkPolys();
    std::puts("Doom FX: 72 weapon/material templates, screen particles, moving GPU emitters, "
              "freeze, bounded effects, "
              "depth-projector bases/lifetimes/fades/recycling and reload passed.");
}
