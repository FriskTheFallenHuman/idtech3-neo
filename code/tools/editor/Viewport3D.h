// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "UI.h"
#include "../../renderer/editor_render.h"
namespace de {
const EditorRenderAPI &editorRenderer();
void shutdown3D();
void invalidate3DTextures();
void drawViewport3D(App &, ImVec2 size);
} // namespace de
