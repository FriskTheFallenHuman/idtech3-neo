// Persistent emitter motion and lifetime stay in the VFX simulation. The GPU
// integrates a world-space, advected procedural fire field into the particle buffer.
#include "common_headers.h"
#include "FxFlame.h"

CFlame::CFlame(const vec3_t origin, const vec3_t velocity, float radius, const vec3_t color,
               qhandle_t shader, int key) {
    VectorCopy(origin, mOrigin1);
    VectorCopy(velocity, drift);
    VectorCopy(color, tint);
    scale = Com_Clamp(.4f, 24, radius);
    simulationTime = 0;
    lastUpdate = theFxHelper.mTime;
    emitterKey = key;
    unsigned int seed = static_cast<unsigned int>(
        static_cast<int>(origin[0] * 13 + origin[1] * 37 + origin[2] * 71));
    seed ^= static_cast<unsigned int>(key) * 2654435761u;
    phase = (seed % 1021) * .00615f;
    mFlags = 0;
    mRefEnt.customShader = shader;
}

bool CFlame::Matches(const vec3_t origin, float radius, const vec3_t color, qhandle_t shader,
                     int key) const {
    return emitterKey == key && mRefEnt.customShader == shader && fabsf(scale - radius) < .01f &&
           DistanceSquared(color, tint) < .001f &&
           (key > 0 || DistanceSquared(origin, mOrigin1) < .01f);
}

void CFlame::Feed(const vec3_t origin, const vec3_t velocity) {
    VectorCopy(origin, mOrigin1);
    VectorCopy(velocity, drift);
}

void CFlame::Draw() {
    vec3_t offset;
    VectorSubtract(mOrigin1, cg.refdef.vieworg, offset);
    if (DotProduct(offset, cg.refdef.viewaxis[0]) < -scale * 8 ||
        VectorLengthSquared(offset) > 1600 * 1600) {
        return;
    }
    refEntity_t entity = {};
    entity.reType = RT_SCREEN_FLAME;
    VectorCopy(mOrigin1, entity.origin);
    entity.radius = scale;
    entity.customShader = mRefEnt.customShader;
    entity.effectTime = simulationTime;
    entity.effectSeed = phase;
    for (int axis = 0; axis < 3; ++axis) {
        entity.shaderRGBA[axis] = static_cast<::byte>(255 * Com_Clamp(0, 1, tint[axis]));
    }
    entity.shaderRGBA[3] =
        static_cast<::byte>(255 * Com_Clamp(0, 1, (mTimeEnd - theFxHelper.mTime) / 80.0f));
    trap_R_AddRefEntityToScene(&entity);
    extern int drawnFx;
    ++drawnFx;
}

bool CFlame::Update() {
    int elapsed = theFxHelper.mTime - lastUpdate;
    lastUpdate = theFxHelper.mTime;
    if (elapsed < 0) {
        return false;
    }
    simulationTime += elapsed * .001f;
    VectorMA(mOrigin1, elapsed * .001f, drift, mOrigin1);
    Draw();
    return true;
}
