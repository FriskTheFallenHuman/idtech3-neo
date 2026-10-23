/* GPL v3 or later. See COPYING.txt. */
#ifndef TR_HBAO_H
#define TR_HBAO_H
#include "tr_glprogs.h"
struct cvar_s;

// Heretic's horizon search and bilateral composite, adapted to OpenGL depth.
class idHBAO {
  public:
    void Init();
    void Shutdown();
    bool Enabled() const;
    void Draw();
    void Info() const;

  private:
    void Resize(int width, int height);
    idGLProgram evaluate, composite;
    idGLFramebuffer framebuffer;
    struct Uniforms {
        int sceneSize, projection, settings, screenOffset;
    } evaluateUniforms{}, compositeUniforms{};
    cvar_s *enable = nullptr, *strength = nullptr, *radius = nullptr;
    unsigned ambient = 0, draws = 0;
    int width = 0, height = 0;
};
extern idHBAO hbao;
#endif
