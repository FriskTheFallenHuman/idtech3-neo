// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../game/q_shared.h"
#include "../qcommon.h"
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>

namespace {
std::map<std::string, std::string> cvars;
int fileLength = -1, closedFiles = 0, registeredFlags = 0;
bool openFile = false;
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int Q_stricmp(const char *a, const char *b) {
    return _stricmp(a, b);
}
int FS_FOpenFileRead(const char *path, fileHandle_t *file, qboolean unique) {
    Check(std::string(path) == "wad/DOOM.wad" && !unique,
          "Content must use the mounted filesystem's wad path");
    *file = openFile ? 1 : 0;
    return fileLength;
}
void FS_FCloseFile(fileHandle_t file) {
    Check(file == 1, "Only opened handles may be closed");
    ++closedFiles;
}
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
    registeredFlags = flags;
    cvars.emplace(name, value);
    return nullptr;
}
void Cvar_Set(const char *name, const char *value) {
    cvars[name] = value;
}
void QDECL Com_Printf(const char *, ...) {
}

int main() {
    try {
        cvars["doom_demo"] = "0";
        FS_InitDoomContent();
        Check(cvars["doom_demo"] == "1" && (registeredFlags & CVAR_ROM) && !closedFiles,
              "Missing WAD must force read-only demo mode despite command-line values");
        const char *restricted[] = {"e2m1",          "e2m9",           "e3m1",
                                    "e3m9",          "E2M5",           "maps/e2m1.bsp",
                                    "MAPS/E3M9.BSP", "maps\\e3m4.bsp", "maps/../maps/e2m6.bsp"};
        for (const char *map : restricted) {
            Check(!FS_DoomMapAllowed(map), "Demo must reject every registered map path");
        }
        const char *allowed[] = {"",
                                 "e",
                                 "e2",
                                 "e2m",
                                 "e1m1",
                                 "e1m9",
                                 "maps/E1M9.BSP",
                                 "testroom",
                                 "e2m1_custom",
                                 "maps/e2m1_custom.bsp"};
        for (const char *map : allowed) {
            Check(FS_DoomMapAllowed(map), "Episode 1 and unrelated custom maps must remain usable");
        }
        openFile = true;
        fileLength = 0;
        FS_InitDoomContent();
        Check(cvars["doom_demo"] == "1" && closedFiles == 1,
              "Empty WAD must remain demo and close its handle");
        fileLength = -1;
        FS_InitDoomContent();
        Check(cvars["doom_demo"] == "1" && closedFiles == 2,
              "Failed size query must remain demo and close its handle");
        fileLength = 12408292;
        FS_InitDoomContent();
        Check(cvars["doom_demo"] == "0" && closedFiles == 3,
              "Readable installed WAD must enable registered mode and close its handle");
        for (const char *map : restricted) {
            Check(FS_DoomMapAllowed(map), "Registered mode must allow all campaign maps");
        }
        fileLength = -1;
        openFile = false;
        Check(FS_DoomMapAllowed("e2m1"), "Access must remain stable until the next startup");
        FS_InitDoomContent();
        cvars["doom_demo"] = "0";
        Check(!FS_DoomMapAllowed("e2m1"), "Cvar changes cannot bypass engine enforcement");
        std::puts("Doom filesystem detection, handle lifetime and episode access checks passed.");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
