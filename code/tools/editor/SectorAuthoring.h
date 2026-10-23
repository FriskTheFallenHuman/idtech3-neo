// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BlockWorld.h"

namespace de {
// A complete path is committed as one undo step. Both input windings produce
// inward-facing one-sided walls. Shared boundaries become portals atomically.
void authorSector(EditWorld &world, const std::vector<Vec2> &polygon,
                  const Line &prototype = Line{});
void authorDivider(EditWorld &world, const std::vector<Vec2> &path);
std::vector<std::pair<int, int>> sectorAt(const EditWorld &world, Vec2 point);
void applySurfaceTexture(EditWorld &world, const std::vector<std::pair<int, int>> &sides,
                         const std::string &texture, int surface);
} // namespace de
