// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/game/q_shared.h"
#include "game/game/bg_public.h"
#include "qcommon/qcommon.h"
#include "game/cgame/cg_public.h"
#include "game/game/bg_doommonsters.h"
#include <cstdio>
#include <stdexcept>

cvar_t *cl_shownet;
void Com_Memset(void *dest, const int value, const size_t count) {
    memset(dest, value, count);
}
void Com_Memcpy(void *dest, const void *source, const size_t count) {
    memcpy(dest, source, count);
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_DPrintf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}

int main() {
    try {
        static_assert(MAX_CLIENTS == 256 && MAX_SP_CLIENTS == 256, "Matching client capacity");
        static_assert(MAX_ENTITIES_IN_SNAPSHOT >= MAX_GENTITIES,
                      "No crowded-map snapshot truncation");
        byte bytes[MAX_MSGLEN];
        msg_t message;
        MSG_Init(&message, bytes, sizeof(bytes));
        entityState_t baseline{}, actor{}, decoded{};
        actor.number = 255;
        actor.clientNum = 255;
        actor.eType = ET_PLAYER;
        actor.eFlags = EF_DEAD;
        actor.density = doommonster::NetworkBase + doommonster::SpiderMastermind;
        actor.legsAnim = doommonster::Death;
        actor.effect1Time = 12345678;
        actor.pos.trBase[0] = -16384.5f;
        actor.apos.trBase[1] = 315;
        MSG_WriteDeltaEntity(&message, &baseline, &actor, qtrue);
        MSG_BeginReading(&message);
        const int number = MSG_ReadBits(&message, GENTITYNUM_BITS);
        MSG_ReadDeltaEntity(&message, &baseline, &decoded, number);
        if (decoded.number != 255 || decoded.clientNum != 255 || decoded.density != actor.density ||
            decoded.legsAnim != actor.legsAnim || decoded.effect1Time != actor.effect1Time ||
            decoded.pos.trBase[0] != actor.pos.trBase[0] || decoded.apos.trBase[1] != 315 ||
            decoded.eFlags != EF_DEAD) {
            throw std::runtime_error(
                "Monster snapshot state did not survive the real network codec");
        }
        playerState_t state{}, restored{};
        entityState_t impact{}, restoredImpact{};
        impact.number = 500;
        impact.eType = ET_EVENTS + EV_MISSILE_MISS;
        impact.weapon = WP_DOOM_PISTOL;
        impact.otherEntityNum = ENTITYNUM_WORLD;
        impact.effect2Time = SURF_METAL | SURF_GLASS | SURF_NOMARKS;
        impact.effect1Time = CONTENTS_SLIME | CONTENTS_SOLID;
        VectorSet(impact.origin2, .6f, .8f, 0);
        VectorSet(impact.angles, 147.25f, -56.5f, 32);
        MSG_Clear(&message);
        MSG_WriteDeltaEntity(&message, &baseline, &impact, qtrue);
        MSG_BeginReading(&message);
        const int impactNumber = MSG_ReadBits(&message, GENTITYNUM_BITS);
        MSG_ReadDeltaEntity(&message, &baseline, &restoredImpact, impactNumber);
        if (restoredImpact.effect2Time != impact.effect2Time ||
            restoredImpact.effect1Time != impact.effect1Time ||
            DistanceSquared(restoredImpact.origin2, impact.origin2) > .000001f ||
            DistanceSquared(restoredImpact.angles, impact.angles) > .000001f ||
            restoredImpact.otherEntityNum != ENTITYNUM_WORLD ||
            restoredImpact.weapon != WP_DOOM_PISTOL) {
            throw std::runtime_error("VFX surface flags/contents lost bits in the network codec");
        }
        state.clientNum = 255;
        state.stats[STAT_DOOM_MONSTER] = doommonster::SpiderMastermind;
        state.stats[STAT_HEALTH] = 3000;
        MSG_Clear(&message);
        MSG_WriteDeltaPlayerstate(&message, nullptr, &state);
        MSG_BeginReading(&message);
        MSG_ReadDeltaPlayerstate(&message, nullptr, &restored);
        if (restored.clientNum != 255 ||
            restored.stats[STAT_DOOM_MONSTER] != doommonster::SpiderMastermind ||
            restored.stats[STAT_HEALTH] != 3000) {
            throw std::runtime_error("High-slot player state did not round-trip");
        }
        std::puts("Client 255, monster sprite state and player state round-trip through the "
                  "network codec.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
