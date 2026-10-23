#pragma once
#include "EditWorld.h"
namespace de {
struct ConnectedSector {
    SectorDef def;
    std::vector<std::pair<int, int>> sides;
};
std::vector<ConnectedSector> connectSectors(EditWorld &world, bool joinSharedWalls = false);
// Atomically split/merge shared boundaries; caller owns the undo checkpoint.
int joinAdjacentSectors(EditWorld &world);
// Finish a newly drawn connection without adding a second undo checkpoint.
bool finishDrawnSector(EditWorld &world, int lastLine, bool closedLoop);
int lineByPoint(const EditWorld &world, Vec2 p, int &side);
std::vector<std::pair<int, int>> makeInnerSector(EditWorld &world, Vec2 cursor);
} // namespace de
