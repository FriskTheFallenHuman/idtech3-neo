// SPDX-License-Identifier: GPL-3.0-or-later
#include "qcommon/doom_save.h"
#include <cstdio>
#include <initializer_list>
#include <stdexcept>

int main() {
    try {
        byte bytes[doomsave::HeaderSize]{};
        int version = SAVE_VERSION, time = 4567;
        std::memcpy(bytes, &version, sizeof(version));
        std::memcpy(bytes + sizeof(int), "e1m5", 5);
        std::memcpy(bytes + sizeof(int) + MAX_QPATH, &time, sizeof(time));
        char map[MAX_QPATH];
        int restoredTime;
        const char *error;
        auto valid = [&](int size) {
            return doomsave::ReadHeader(bytes, size, map, restoredTime, error);
        };
        if (!valid(sizeof(bytes)) || std::strcmp(map, "e1m5") || restoredTime != time) {
            throw std::runtime_error("Valid legacy save header changed");
        }
        for (int size = 0; size < doomsave::HeaderSize; ++size) {
            if (valid(size) || !error) {
                throw std::runtime_error("Truncated save header accepted");
            }
        }
        for (const char *name : {"", "../e1m5", "e1m5%p", "e1m5\nquit", "/e1m5", "a//b", "a/"}) {
            std::memset(bytes + sizeof(int), 0, MAX_QPATH);
            std::memcpy(bytes + sizeof(int), name, std::strlen(name));
            if (valid(sizeof(bytes)) || !error) {
                throw std::runtime_error("Unsafe save map name accepted");
            }
        }
        std::memset(bytes + sizeof(int), 'x', MAX_QPATH);
        if (valid(sizeof(bytes))) {
            throw std::runtime_error("Unterminated save map name accepted");
        }
        std::memcpy(bytes + sizeof(int), "e1m5", 5);
        version = SAVE_VERSION - 1;
        std::memcpy(bytes, &version, sizeof(version));
        if (valid(sizeof(bytes))) {
            throw std::runtime_error("Incompatible save version accepted");
        }
        version = SAVE_VERSION;
        std::memcpy(bytes, &version, sizeof(version));
        time = -1;
        std::memcpy(bytes + sizeof(int) + MAX_QPATH, &time, sizeof(time));
        if (valid(sizeof(bytes))) {
            throw std::runtime_error("Negative save time accepted");
        }
        std::puts("Save header validation checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
