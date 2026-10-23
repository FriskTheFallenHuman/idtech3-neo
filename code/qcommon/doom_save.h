// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DOOM_SAVE_H
#define DOOM_SAVE_H

#include "../game/q_shared.h"
#include <cstring>

namespace doomsave {
constexpr int HeaderSize = 2 * sizeof(int) + MAX_QPATH;
static_assert(sizeof(int) == 4, "Save headers require 32-bit integers");

// Version 22 retains the existing little-endian Win32 save layout. Validate its
// routing fields before treating file data as a map name or changing servers.
inline bool ReadHeader(const void *data, int size, char (&map)[MAX_QPATH], int &time,
                       const char *&error) {
    error = nullptr;
    if (!data || size < HeaderSize) {
        error = "truncated save header";
        return false;
    }
    const auto *bytes = static_cast<const byte *>(data);
    int version;
    std::memcpy(&version, bytes, sizeof(version));
    if (version != SAVE_VERSION) {
        error = "unsupported save version";
        return false;
    }
    std::memcpy(map, bytes + sizeof(int), MAX_QPATH);
    if (!std::memchr(map, 0, MAX_QPATH) || !map[0]) {
        error = "invalid save map name";
        return false;
    }
    for (int index = 0; map[index]; ++index) {
        const char value = map[index];
        if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '_' || value == '-' ||
              (value == '/' && index > 0 && map[index - 1] != '/' && map[index + 1]))) {
            error = "invalid save map name";
            return false;
        }
    }
    std::memcpy(&time, bytes + sizeof(int) + MAX_QPATH, sizeof(time));
    if (time < 0) {
        error = "negative save time";
        return false;
    }
    return true;
}
} // namespace doomsave
#endif
