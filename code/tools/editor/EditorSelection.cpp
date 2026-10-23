// SPDX-License-Identifier: GPL-3.0-or-later
#include "DesktopPanels.h"
namespace de {
namespace {
int activeTool = 0, activeSide = 0;
}
int desktopTool() {
    return activeTool;
}
int &desktopLineSide() {
    return activeSide;
}
void setDesktopTool(App &a, int tool) {
    activeTool = tool;
    a.view.tool = tool;
    a.view.drawing = false;
}
void applyDesktopLines(App &a, const std::function<void(Line &)> &edit) {
    a.world.checkpoint();
    for (auto &line : a.world.lines) {
        if (line.selected > 0) {
            edit(line);
        }
    }
}
void applyDesktopSectors(App &a, const std::function<void(SectorDef &)> &edit) {
    a.world.checkpoint();
    if (!a.pickedSectorSides.empty()) {
        for (auto [line, side] : a.pickedSectorSides) {
            edit(a.world.lines.at(line).side.at(side).ends);
        }
    } else {
        for (auto &line : a.world.lines) {
            if (line.selected > 0) {
                edit(line.side.at(line.selected - 1).ends);
            }
        }
    }
}
void applyDesktopThings(App &a, const std::function<void(Thing &)> &edit) {
    a.world.checkpoint();
    for (auto &thing : a.world.things) {
        if (thing.selected > 0) {
            edit(thing);
        }
    }
}
} // namespace de
