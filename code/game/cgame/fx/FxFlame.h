// Persistent VFX emitter. The renderer evaluates the emissive field on the GPU.
#ifndef FX_FLAME_H
#define FX_FLAME_H
#include "FxPrimitives.h"

class CFlame : public CEffect {
    vec3_t drift, tint;
    float scale, simulationTime, phase;
    int lastUpdate, emitterKey;
    void Draw();

  public:
    CFlame(const vec3_t origin, const vec3_t velocity, float radius, const vec3_t color,
           qhandle_t shader, int key = 0);
    bool Update() override;
    bool Matches(const vec3_t origin, float radius, const vec3_t color, qhandle_t shader,
                 int key) const;
    void Feed(const vec3_t origin, const vec3_t velocity);
};
void FX_AddFlame(const vec3_t origin, const vec3_t velocity, float radius, const vec3_t color,
                 int life, qhandle_t shader);
#endif
