// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "q_shared.h"
#include <cerrno>
#include <climits>

namespace doomlevel {
enum Phase { Counting, Statistics, Entering, Leaving };
struct Summary {
    int episode, mission, nextEpisode, nextMission;
    int kills, totalKills, items, totalItems, secrets, totalSecrets;
    int seconds, par, visited, phase, started;
};
inline bool Map(const char *name, int &episode, int &mission) {
    if (!name || strlen(name) != 4 || name[0] != 'e' || name[2] != 'm' || name[1] < '1' ||
        name[1] > '3' || name[3] < '1' || name[3] > '9') {
        return false;
    }
    episode = name[1] - '0';
    mission = name[3] - '0';
    return true;
}
inline bool Next(int episode, int mission, bool secret, int &nextEpisode, int &nextMission) {
    if (episode < 1 || episode > 3 || mission < 1 || mission > 9) {
        return false;
    }
    nextEpisode = episode;
    nextMission = secret         ? 9
                  : mission == 9 ? (episode == 1   ? 4
                                    : episode == 2 ? 6
                                                   : 7)
                                 : mission + 1;
    if (!secret && mission == 8) {
        nextEpisode = episode % 3 + 1;
        nextMission = 1;
    }
    return true;
}
inline int Par(int episode, int mission) {
    constexpr int times[3][9] = {{30, 75, 120, 90, 165, 180, 180, 30, 165},
                                 {90, 90, 90, 120, 90, 360, 240, 30, 170},
                                 {90, 45, 90, 150, 90, 90, 165, 30, 135}};
    return episode >= 1 && episode <= 3 && mission >= 1 && mission <= 9
               ? times[episode - 1][mission - 1]
               : 0;
}
inline int Percent(int value, int total) {
    return total > 0 ? static_cast<int>(static_cast<long long>(value) * 100 / total) : 0;
}
inline bool Integers(const char *text, int *values, int count) {
    for (int i = 0; i < count; ++i) {
        while (*text == ' ' || *text == '\t') {
            ++text;
        }
        if (*text < '0' || *text > '9') {
            return false;
        }
        char *end = nullptr;
        errno = 0;
        const long value = strtol(text, &end, 10);
        if (errno == ERANGE || value > INT_MAX) {
            return false;
        }
        values[i] = static_cast<int>(value);
        text = end;
    }
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    return !*text;
}
inline void Write(char *text, int size, const Summary &s) {
    Com_sprintf(text, size, "1 %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", s.episode, s.mission,
                s.nextEpisode, s.nextMission, s.kills, s.totalKills, s.items, s.totalItems,
                s.secrets, s.totalSecrets, s.seconds, s.par, s.visited, s.phase, s.started);
}
inline bool Parse(const char *text, Summary &summary) {
    int v[16];
    if (!Integers(text, v, 16) || v[0] != 1 || v[1] < 1 || v[1] > 3 || v[2] < 1 || v[2] > 9 ||
        v[3] < 1 || v[3] > 3 || v[4] < 1 || v[4] > 9 || v[13] > 511 || v[14] > Leaving) {
        return false;
    }
    for (int i = 5; i <= 10; ++i) {
        if (v[i] > 65535) {
            return false;
        }
    }
    summary = {v[1], v[2],  v[3],  v[4],  v[5],  v[6],  v[7], v[8],
               v[9], v[10], v[11], v[12], v[13], v[14], v[15]};
    return true;
}
} // namespace doomlevel
