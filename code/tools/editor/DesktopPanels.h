#pragma once
#include "UI.h"
namespace de {
bool drawDesktopPanel(App &, const std::string &key);
int desktopTool();
void setDesktopTool(App &, int);
int &desktopLineSide();
void applyDesktopLines(App &, const std::function<void(Line &)> &);
void applyDesktopSectors(App &, const std::function<void(SectorDef &)> &);
void applyDesktopThings(App &, const std::function<void(Thing &)> &);
} // namespace de
