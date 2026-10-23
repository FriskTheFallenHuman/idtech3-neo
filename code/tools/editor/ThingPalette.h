#pragma once
#include "UI.h"
namespace de {
std::string placementObjectName(const App &app);
void choosePlacementObject(App &app, const ThingDef &definition);
const char *objectCategory(int type);
void drawObjectThumbnail(App &app, const ThingDef &definition, ImVec2 size);
} // namespace de
