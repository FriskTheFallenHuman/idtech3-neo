/*
===========================================================================
Hexen II port of the RTCW single-player GPL source.
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Distributed under the GNU General Public License, version 3 or later.
See COPYING.txt and the source distribution's additional terms.
===========================================================================
*/

#ifndef TR_VFX_H
#define TR_VFX_H

void R_InitVfx();
void R_ShutdownVfx();
bool R_VfxAvailable();
bool R_IsScreenParticle(const refEntity_t &entity);
bool RB_ScreenEntityVisible(const refEntity_t &entity);
void RB_BeginVfxView(bool requireDepth = false);
void RB_CaptureVfxDepth();
void RB_RenderVfx();
void RB_RenderFlames();
void RB_DrawFlameCoronas();
void RB_BindVfxTexture(int unit, unsigned int texture);
void RB_DrawVfxQuad(const float *rectangle);
bool RB_VfxBounds(const vec3_t origin, float radius, float height, float *rectangle);
void RB_VfxViewProjection(float *matrix);
bool RB_VfxInverseMatrix(const float *matrix, float *inverse);
unsigned int RB_VfxDepthTexture();
bool RB_VfxDepthReady();
void RB_VfxRestoreState();

#endif
