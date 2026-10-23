// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Interior flood seeds include disconnected Doom sectors, independently of
// human starts, so every monster size retains its reachable map components.
extern bool bspcDoomMap;
extern bool bspcDoomHasInterior;
extern int bspcDoomSeedCount;
extern float bspcDoomSeeds[32768][3];

void BSPC_DoomAddSeal();
