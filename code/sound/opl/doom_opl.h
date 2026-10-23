// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstddef>
bool DoomOPL_Reset(const uint8_t *data, size_t size);
void DoomOPL_Event(uint8_t status, uint8_t first, uint8_t second);
void DoomOPL_Render(int16_t *stereo, size_t frames);
