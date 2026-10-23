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

namespace {
idGLProgram decalProgram;
cvar_t *decalDebug;
int previousProjected;
struct decalUniforms_t {
    int inverseViewProjection, screenSize, screenOffset, origin, normal, tangent, bitangent, radius,
        thickness;
} uniforms;
} // namespace

void R_InitDecals() {
    decalDebug = ri.Cvar_Get("r_vfxDebug", "0", 0);
    previousProjected = 0;
    decalProgram.Load("glprogs/screen.vert", "glprogs/decal.frag");
    decalProgram.Bind();
    decalProgram.SetInt(decalProgram.Uniform("uDecal"), 0);
    decalProgram.SetInt(decalProgram.Uniform("uSceneDepth"), 1);
#define DECAL_UNIFORM(field, name) uniforms.field = decalProgram.Uniform(name)
    DECAL_UNIFORM(inverseViewProjection, "uInverseViewProjection");
    DECAL_UNIFORM(screenSize, "uScreenSize");
    DECAL_UNIFORM(screenOffset, "uScreenOffset");
    DECAL_UNIFORM(origin, "uOrigin");
    DECAL_UNIFORM(normal, "uNormal");
    DECAL_UNIFORM(tangent, "uTangent");
    DECAL_UNIFORM(bitangent, "uBitangent");
    DECAL_UNIFORM(radius, "uRadius");
    DECAL_UNIFORM(thickness, "uThickness");
#undef DECAL_UNIFORM
    idGLProgram::Unbind();
}

void R_ShutdownDecals() {
    decalProgram.Shutdown();
}

void RB_RenderDecals() {
    if (!RB_VfxDepthReady()) {
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
    decalProgram.Bind();
    RB_BindVfxTexture(1, RB_VfxDepthTexture());
    decalProgram.SetMatrix(uniforms.inverseViewProjection, inverse);
    decalProgram.SetVec2(uniforms.screenSize, backEnd.viewParms.viewportWidth,
                         backEnd.viewParms.viewportHeight);
    decalProgram.SetVec2(uniforms.screenOffset, backEnd.viewParms.viewportX,
                         backEnd.viewParms.viewportY);
    int projected = 0;
    // Oldest first: overlapping fresh blood should appear on top of older marks.
    for (int index = backEnd.refdef.num_entities - 1; index >= 0; --index) {
        const refEntity_t &entity = backEnd.refdef.entities[index].e;
        if (entity.reType != RT_SCREEN_DECAL || !RB_ScreenEntityVisible(entity)) {
            continue;
        }
        vec3_t delta;
        VectorSubtract(entity.origin, backEnd.viewParms.or.origin, delta);
        if (DotProduct(delta, backEnd.viewParms.or.axis[0]) < -entity.radius * 2) {
            continue;
        }
        float rectangle[4];
        float bound = entity.radius * 1.5f + entity.effectThickness;
        if (!RB_VfxBounds(entity.origin, bound, bound, rectangle)) {
            continue;
        }
        decalProgram.SetVec3(uniforms.origin, entity.origin);
        decalProgram.SetVec3(uniforms.normal, entity.axis[0]);
        decalProgram.SetVec3(uniforms.tangent, entity.axis[1]);
        decalProgram.SetVec3(uniforms.bitangent, entity.axis[2]);
        decalProgram.SetFloat(uniforms.radius, entity.radius);
        decalProgram.SetFloat(uniforms.thickness, entity.effectThickness);
        qglColor4ubv(entity.shaderRGBA);
        shader_t *shader = R_GetShaderByHandle(entity.customShader);
        for (int stageIndex = 0; stageIndex < shader->numUnfoggedPasses; ++stageIndex) {
            const shaderStage_t *stage = shader->stages[stageIndex];
            image_t *image = stage->bundle[0].image[0];
            if (!image) {
                continue;
            }
            RB_BindVfxTexture(0, image->texnum);
            image->frameUsed = tr.frameCount;
            unsigned blend = stage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
            if (!blend) {
                blend = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
            }
            GL_State(GLS_DEPTHTEST_DISABLE | blend);
            RB_DrawVfxQuad(rectangle);
            ++projected;
        }
    }
    idGLProgram::Unbind();
    GL_SelectTexture(saved.currenttmu);
    qglPopAttrib();
    glState = saved;
    if (decalDebug->integer && projected && projected != previousProjected) {
        ri.Printf(PRINT_ALL, "Screen decals: %d projected\n", projected);
    }
    previousProjected = projected;
}
