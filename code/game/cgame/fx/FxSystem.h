
#ifndef FX_SYSTEM_H_INC
#define FX_SYSTEM_H_INC
#include "../cg_local.h"

#define irand FX_IRand
#define flrand FX_FRand

struct fxRefEntity_t : refEntity_t {
    vec3_t angles;
};
int FX_IRand(int min, int max);
float FX_FRand(float min, float max);
void FX_DrawRibbon(const refEntity_t &ent, const vec3_t start, const vec3_t end, float startWidth,
                   float endWidth);
void FX_DrawQuad(const refEntity_t &ent, const vec3_t normal);
void FX_DrawCylinder(const refEntity_t &ent);
extern vmCvar_t fx_debug;
extern vmCvar_t fx_freeze;
extern vmCvar_t fx_expensivePhysics;

inline void Vector2Clear(vec2_t a) {
    a[0] = 0.0f;
    a[1] = 0.0f;
}

inline void Vector2Set(vec2_t a, float b, float c) {
    a[0] = b;
    a[1] = c;
}

inline void Vector2Copy(vec2_t src, vec2_t dst) {
    dst[0] = src[0];
    dst[1] = src[1];
}

struct SFxHelper {
    int mTime;
    int mFrameTime;
    int mEmitterKey; // stable identity for continuously fed fluid sources
    float mFloatFrameTime;

    void Init();
    void AdjustTime(int time);

    // These functions are wrapped and used by the fx system in case it makes things a bit more portable
    void Print(const char *msg, ...);

    // File handling
    int OpenFile(const char *path, fileHandle_t *fh, int mode);
    int ReadFile(void *data, int len, fileHandle_t fh);
    void CloseFile(fileHandle_t fh);

    // Sound
    void PlaySound(const vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx);
    void PlayLocalSound(sfxHandle_t sfx, int channelNum);
    int RegisterSound(const char *sound);

#ifdef _IMMERSION
    void PlayForce(int entityNum, ffHandle_t ff);
    ffHandle_t RegisterForce(const char *force, int channel);
#endif // _IMMERSION
    // MD3 attachments
    int GetEntityAttachment(const centity_t &cent, int modelNum, int boltNum, vec3_t /*out*/ origin,
                            vec3_t /*out*/ *axis);

    // Physics/collision
    void Trace(trace_t *tr, vec3_t start, vec3_t min, vec3_t max, vec3_t end, int skipEntNum,
               int flags);

    void AddFxToScene(refEntity_t *ent);
    void AddLightToScene(vec3_t org, float radius, float red, float green, float blue);

    int RegisterShader(const char *shader);
    int RegisterModel(const char *model);

    void AddPolyToScene(int shader, int count, polyVert_t *verts);

    void CameraShake(vec3_t origin, float intensity, int radius, int time);
};

extern SFxHelper theFxHelper;

#endif // FX_SYSTEM_H_INC
