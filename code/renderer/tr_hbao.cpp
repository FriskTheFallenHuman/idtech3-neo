/* GPL v3 or later. See COPYING.txt. */
#include "tr_local.h"
#include "tr_hbao.h"
#include "tr_vfx.h"

namespace {
void HBAO_Info_f() {
    R_SyncRenderThread();
    hbao.Info();
}
} // namespace
idHBAO hbao;

void idHBAO::Init() {
    enable = ri.Cvar_Get("r_hbao", "1", CVAR_ARCHIVE);
    strength = ri.Cvar_Get("r_hbao_strength", "60", CVAR_ARCHIVE);
    radius = ri.Cvar_Get("r_hbao_radius", "60", CVAR_ARCHIVE);
    evaluate.Load("glprogs/screen.vert", "glprogs/hbao_evaluate.frag");
    composite.Load("glprogs/screen.vert", "glprogs/hbao_composite.frag");
    auto uniforms = [](const idGLProgram &program, Uniforms &locations) {
        program.Bind();
        program.SetInt(program.Uniform("uSceneDepth"), 0);
        program.SetInt(program.Uniform("uAmbient"), 1);
        locations.sceneSize = program.Uniform("uSceneSize");
        locations.projection = program.Uniform("uProjection");
        locations.settings = program.Uniform("uSettings");
        locations.screenOffset = program.Uniform("uScreenOffset");
    };
    uniforms(evaluate, evaluateUniforms);
    uniforms(composite, compositeUniforms);
    idGLProgram::Unbind();
    ri.Cmd_AddCommand("hbaoinfo", HBAO_Info_f);
}
void idHBAO::Shutdown() {
    framebuffer.Shutdown();
    evaluate.Shutdown();
    composite.Shutdown();
    if (ambient) {
        qglDeleteTextures(1, &ambient);
    }
    ambient = draws = 0;
    width = height = 0;
    enable = strength = radius = nullptr;
    ri.Cmd_RemoveCommand("hbaoinfo");
}
bool idHBAO::Enabled() const {
    return enable && enable->integer && strength->value > 0 && R_VfxAvailable();
}
void idHBAO::Resize(int sceneWidth, int sceneHeight) {
    const int nextWidth = (sceneWidth + 1) / 2, nextHeight = (sceneHeight + 1) / 2;
    if (width == nextWidth && height == nextHeight) {
        return;
    }
    framebuffer.Shutdown();
    if (!ambient) {
        qglGenTextures(1, &ambient);
    }
    RB_BindVfxTexture(1, ambient);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812f /* CLAMP_TO_EDGE */);
    qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812f);
    // RG stores visibility and linear depth. RGBA16F shares the VFX capability floor.
    qglTexImage2D(GL_TEXTURE_2D, 0, 0x881a /* RGBA16F */, nextWidth, nextHeight, 0, GL_RGBA,
                  GL_FLOAT, nullptr);
    framebuffer.Create(ambient);
    width = nextWidth;
    height = nextHeight;
}
void idHBAO::Draw() {
    if (!Enabled() || (backEnd.refdef.rdflags & RDF_NOWORLDMODEL) || !RB_VfxDepthReady() ||
        !RB_VfxDepthTexture()) {
        return;
    }
    const glstate_t saved = glState;
    qglPushAttrib(GL_ALL_ATTRIB_BITS);
    qglDisable(GL_FOG);
    qglDisable(GL_CLIP_PLANE0);
    GL_Cull(CT_TWO_SIDED);
    GL_State(GLS_DEPTHTEST_DISABLE);
    qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    qglColor4f(1, 1, 1, 1);
    const auto &view = backEnd.viewParms;
    Resize(view.viewportWidth, view.viewportHeight);
    RB_BindVfxTexture(0, RB_VfxDepthTexture());
    const float projection[] = {view.projectionMatrix[0], view.projectionMatrix[5],
                                view.projectionMatrix[10], view.projectionMatrix[14]};
    const float settings[] = {Com_Clamp(1, 128, radius->value),
                              Com_Clamp(0, 200, strength->value) / 100.0f, .1f};
    auto parameters = [&](const idGLProgram &program, const Uniforms &locations) {
        program.Bind();
        program.SetVec2(locations.sceneSize, float(view.viewportWidth), float(view.viewportHeight));
        program.SetVec4(locations.projection, projection);
        program.SetVec3(locations.settings, settings);
        program.SetVec2(locations.screenOffset, float(view.viewportX), float(view.viewportY));
    };
    framebuffer.Bind();
    qglViewport(0, 0, width, height);
    qglScissor(0, 0, width, height);
    parameters(evaluate, evaluateUniforms);
    const float fullView[] = {-1, -1, 1, 1};
    RB_DrawVfxQuad(fullView);
    idGLFramebuffer::BindDefault();
    qglViewport(view.viewportX, view.viewportY, view.viewportWidth, view.viewportHeight);
    qglScissor(view.viewportX, view.viewportY, view.viewportWidth, view.viewportHeight);
    RB_BindVfxTexture(1, ambient);
    parameters(composite, compositeUniforms);
    GL_State(GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO);
    qglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    RB_DrawVfxQuad(fullView);
    idGLProgram::Unbind();
    GL_SelectTexture(saved.currenttmu);
    qglPopAttrib();
    glState = saved;
    draws++;
}
void idHBAO::Info() const {
    ri.Printf(PRINT_ALL,
              "HBAO: enabled=%d, strength=%.0f%%, radius=%.1f units, %dx%d half-resolution "
              "buffer, %u views\n",
              Enabled(), strength ? Com_Clamp(0, 200, strength->value) : 0,
              radius ? Com_Clamp(1, 128, radius->value) : 0, width, height, draws);
}
