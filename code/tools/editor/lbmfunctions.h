#pragma once
#include "R_mapdef.h"
#include <filesystem>
namespace de {
Image loadLBM(const std::filesystem::path &p);
void saveLBM(const std::filesystem::path &p, const Bytes &indices, int width, int height,
             const Bytes &palette);
} // namespace de
