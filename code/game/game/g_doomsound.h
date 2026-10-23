// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace doomsound {
struct Link {
    int next, sector;
    bool blocking;
};

// Original P_RecursiveSound semantics: closed openings stop noise, and a path
// may cross one sound-blocking linedef. Caller owns map-lifetime scratch arrays.
inline void Flood(int start, int count, const int *heads, const Link *links, const float *floors,
                  const float *ceilings, int *levels, int *queue) {
    for (int i = 0; i < count; ++i) {
        levels[i] = 2;
    }
    if (start < 0 || start >= count) {
        return;
    }
    int head = 0, tail = 1;
    queue[0] = start;
    levels[start] = 0;
    while (head < tail) {
        const int from = queue[head++];
        for (int index = heads[from]; index >= 0; index = links[index].next) {
            const auto &link = links[index];
            const int to = link.sector;
            const int cost = levels[from] + (link.blocking ? 1 : 0);
            if (cost >= levels[to] || cost > 1 ||
                min(ceilings[from], ceilings[to]) <= max(floors[from], floors[to])) {
                continue;
            }
            levels[to] = cost;
            // Each sector's level can improve at most twice (2 -> 1 -> 0).
            queue[tail++] = to;
        }
    }
}
} // namespace doomsound
