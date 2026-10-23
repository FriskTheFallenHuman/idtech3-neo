#pragma once
#include "R_mapdef.h"
namespace de {
float distanceToLine(Vec2 p, Vec2 a, Vec2 b);
bool lineInRect(Vec2 a, Vec2 b, Vec2 min, Vec2 max);
bool pointInPolygon(Vec2 p, const std::vector<Vec2> &polygon);
float cross(Vec2 a, Vec2 b);
} // namespace de
