// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "R_mapdef.h"
#include <filesystem>
namespace de::sprite_baker {
Image decodeImage(const void *, size_t);
Image readImage(const std::filesystem::path &);
void writePNG(const std::filesystem::path &, const Image &);
std::filesystem::path packsDirectory();
} // namespace de::sprite_baker
