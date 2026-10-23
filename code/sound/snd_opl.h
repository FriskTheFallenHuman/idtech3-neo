// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
namespace doomaudio {
bool OPLLoad(const uint8_t *midi, size_t midiLength, const uint8_t *instruments,
             size_t instrumentLength);
void OPLStop();
bool OPLLoaded();
size_t OPLRender(int16_t *stereo, size_t frames);
unsigned OPLPosition();
unsigned OPLDuration();
const char *OPLError();
} // namespace doomaudio
