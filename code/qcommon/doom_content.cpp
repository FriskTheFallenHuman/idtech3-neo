// SPDX-License-Identifier: GPL-3.0-or-later
#include "../game/q_shared.h"
#include "qcommon.h"

namespace {
bool demoMode = true;
}

void FS_InitDoomContent() {
    // Use the mounted Quake filesystem, including its normal base/home/mod search order.
    // Cache this once at startup so a remote server's pure list cannot change ownership.
    fileHandle_t file = 0;
    const int length = FS_FOpenFileRead("wad/DOOM.wad", &file, qfalse);
    demoMode = !file || length <= 0;
    if (file) {
        FS_FCloseFile(file);
    }
    Cvar_Get("doom_demo", demoMode ? "1" : "0", CVAR_ROM);
    Cvar_Set("doom_demo", demoMode ? "1" : "0");
    Com_Printf("Doom content: %s (wad/DOOM.wad %s)\n",
               demoMode ? "restricted demo: episode 1 only" : "registered: episodes 1-3",
               demoMode ? "missing, empty or unreadable" : "found");
}

qboolean FS_DoomMapAllowed(const char *name) {
    if (!demoMode || !name) {
        return qtrue;
    }
    // Both console names and collision paths reach this boundary. Match the basename
    // case-insensitively, including paths using Windows separators.
    const char *map = name;
    for (const char *p = name; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            map = p + 1;
        }
    }
    const size_t length = strlen(map);
    if ((length == 4 || (length == 8 && !Q_stricmp(map + 4, ".bsp"))) &&
        (map[0] == 'e' || map[0] == 'E') && (map[1] == '2' || map[1] == '3') &&
        (map[2] == 'm' || map[2] == 'M') && map[3] >= '1' && map[3] <= '9') {
        return qfalse;
    }
    return qtrue;
}
