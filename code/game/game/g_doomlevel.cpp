// SPDX-License-Identifier: GPL-3.0-or-later
#include "g_save_callbacks.h"
#include "g_local.h"
#include "g_doomlevel.h"
#include "bg_doomlevel.h"
#include "bg_doomitems.h"
#include "../qcommon/doom_bsp.h"

namespace {
doomlevel::Summary summary{};
int previousButtons, totalKills, totalItems, totalSecrets, frozenAt;
bool active;
// Borrowed from the map adapter; valid until the next map shutdown.
const dbsp::Sector *mapSectors;
int mapSectorCount;
void Publish() {
    char text[512];
    doomlevel::Write(text, sizeof(text), summary);
    trap_SetConfigstring(CS_DOOM_INTERMISSION, text);
    G_Printf("Doom intermission: %s\n", text);
}
bool DemoEnding() {
    return summary.nextEpisode > 1 && trap_Cvar_VariableIntegerValue("doom_demo") != 0;
}
void Advance() {
    if (DemoEnding()) {
        summary.phase = doomlevel::Leaving;
        Publish();
        trap_Cvar_Set("doom_carry", "");
        trap_SendConsoleCommand(EXEC_INSERT, "disconnect\ndoommenu help\n");
        return;
    }
    auto *player = G_FindActor("player");
    if (player && player->client) {
        const auto &ps = player->client->ps;
        int weapons = 0;
        for (int i = 0; i < 8; ++i) {
            if (doomweapon::Owned(ps, WP_DOOM_FIST + i)) {
                weapons |= 1 << i;
            }
        }
        trap_Cvar_Set("doom_carry",
                      va("%d %d %d %d %d %d %d %d %d %d %d %d", summary.nextEpisode,
                         summary.nextMission, max(1, player->health), ps.stats[STAT_ARMOR],
                         ps.stats[STAT_DOOM_ARMOR_TYPE], ps.stats[STAT_DOOM_BACKPACK], weapons,
                         ps.weapon, ps.ammo[WP_DOOM_PISTOL], ps.ammo[WP_DOOM_SHOTGUN],
                         ps.ammo[WP_DOOM_ROCKET], ps.ammo[WP_DOOM_PLASMA]));
    }
    summary.phase = doomlevel::Leaving;
    Publish();
    // Start the destination before any pending console script resumes.
    trap_SendConsoleCommand(EXEC_INSERT,
                            va("spmap e%dm%d\n", summary.nextEpisode, summary.nextMission));
}
} // namespace
void G_DoomLevelInit(const void *data) {
    active = false;
    totalKills = totalItems = totalSecrets = previousButtons = 0;
    trap_SetConfigstring(CS_DOOM_INTERMISSION, "");
    const auto &header = *static_cast<const dbsp::Header *>(data);
    const auto *base = static_cast<const byte *>(data);
    const auto *things =
        reinterpret_cast<const dbsp::Thing *>(base + header.lumps[dbsp::Things].offset);
    const bool multiplayer = g_gametype.integer != GT_SINGLE_PLAYER;
    for (int i = 0; i < header.lumps[dbsp::Things].length / static_cast<int>(sizeof(dbsp::Thing));
         ++i) {
        const auto &thing = things[i];
        if (!doommonster::SpawnAllowed(thing.options, G_DoomSkill(), multiplayer)) {
            continue;
        }
        const int monster = doommonster::TypeForThing(thing.type);
        if (monster > doommonster::None && monster <= doommonster::SpiderMastermind &&
            monster != doommonster::LostSoul) {
            ++totalKills;
        }
        const int item = doomitem::ForThing(thing.type);
        if (item >= 0 && doomitem::definitions[item].countItem) {
            ++totalItems;
        }
    }
    const auto *sectors =
        reinterpret_cast<const dbsp::Sector *>(base + header.lumps[dbsp::Sectors].offset);
    mapSectors = sectors;
    mapSectorCount = header.lumps[dbsp::Sectors].length / sizeof(dbsp::Sector);
    for (int i = 0; i < header.lumps[dbsp::Sectors].length / static_cast<int>(sizeof(dbsp::Sector));
         ++i) {
        totalSecrets += sectors[i].special == 9;
    }
}
DOOM_SAVE_CALLBACK("G_DoomLevelInit", G_DoomLevelInit);

bool G_DoomBeginIntermission(gentity_t *player, bool secret) {
    if (active) {
        return true;
    }
    if (!G_DoomMapActive() || g_gametype.integer != GT_SINGLE_PLAYER || !player ||
        !player->client || player->doomNpc.type) {
        return false;
    }
    char map[MAX_QPATH];
    trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
    summary = {};
    if (!doomlevel::Map(map, summary.episode, summary.mission) ||
        !doomlevel::Next(summary.episode, summary.mission, secret, summary.nextEpisode,
                         summary.nextMission)) {
        return false;
    }
    summary.totalKills = totalKills;
    summary.totalItems = totalItems;
    summary.totalSecrets = totalSecrets;
    for (int i = 1; i < MAX_CLIENTS; ++i) {
        const auto &entity = g_entities[i];
        if (entity.inuse && entity.doomNpc.type > 0 &&
            entity.doomNpc.type <= doommonster::SpiderMastermind &&
            entity.doomNpc.type != doommonster::LostSoul && entity.health <= 0) {
            ++summary.kills;
        }
    }
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        const auto &entity = g_entities[i];
        if (entity.inuse && entity.classname && !Q_stricmp(entity.classname, "doom_sector") &&
            entity.doom.sector >= 0 && entity.doom.sector < mapSectorCount &&
            mapSectors[entity.doom.sector].special == 9 && entity.doom.activated) {
            ++summary.secrets;
        }
    }
    summary.items = player->client->ps.stats[STAT_DOOM_ITEMS];
    summary.seconds =
        max(0, doomProgress.totalPlayTime + level.time - doomProgress.lastLoadTime) / 1000;
    summary.par = doomlevel::Par(summary.episode, summary.mission);
    summary.visited = trap_Cvar_VariableIntegerValue("doom_visited_episode") == summary.episode
                          ? trap_Cvar_VariableIntegerValue("doom_visited") & 511
                          : 0;
    summary.visited |= 1 << (summary.mission - 1);
    trap_Cvar_Set("doom_visited", va("%d", summary.visited));
    trap_Cvar_Set("doom_visited_episode", va("%d", summary.episode));
    active = true;
    frozenAt = level.time;
    summary.started = level.time;
    level.intermissiontime = max(1, level.time);
    previousButtons = player->client->pers.cmd.buttons;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        auto &entity = g_entities[i];
        if (!entity.inuse || !entity.client || entity.doomNpc.type) {
            continue;
        }
        entity.client->ps.pm_type = PM_INTERMISSION;
        entity.client->ps.eFlags &= ~EF_FIRING;
        VectorClear(entity.client->ps.velocity);
        entity.takedamage = qfalse;
    }
    Publish();
    return true;
}
DOOM_SAVE_CALLBACK("G_DoomBeginIntermission", G_DoomBeginIntermission);

bool G_DoomLevelFrame() {
    if (!active) {
        return false;
    }
    auto *player = G_FindActor("player");
    const int buttons = player && player->client ? player->client->pers.cmd.buttons : 0;
    const bool pressed =
        (buttons & ~previousButtons & (BUTTON_ATTACK | BUTTON_ACTIVATE | BUTTON_USE_HOLDABLE)) != 0;
    previousButtons = buttons;
    if (summary.phase == doomlevel::Counting &&
        (level.time - frozenAt >= 10000 || (pressed && level.time - summary.started >= 500))) {
        summary.phase = doomlevel::Statistics;
        summary.started = level.time;
        Publish();
    } else if (summary.phase == doomlevel::Statistics && pressed &&
               level.time - summary.started >= 350) {
        if (DemoEnding()) {
            Advance();
            return true;
        }
        summary.phase = doomlevel::Entering;
        summary.started = level.time;
        Publish();
    } else if (summary.phase == doomlevel::Entering &&
               (level.time - summary.started >= 4000 ||
                (pressed && level.time - summary.started >= 350))) {
        Advance();
    }
    return true;
}
DOOM_SAVE_CALLBACK("G_DoomLevelFrame", G_DoomLevelFrame);

void G_DoomApplyCarry(gentity_t *player) {
    if (!player->client || player->doomNpc.type || !G_DoomMapActive() ||
        g_gametype.integer != GT_SINGLE_PLAYER) {
        return;
    }
    char text[512], map[MAX_QPATH];
    int values[12], episode, mission;
    trap_Cvar_VariableStringBuffer("doom_carry", text, sizeof(text));
    trap_Cvar_Set("doom_carry", "");
    trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
    if (!doomlevel::Map(map, episode, mission) || !doomlevel::Integers(text, values, 12) ||
        values[0] != episode || values[1] != mission || values[2] < 1 || values[2] > 200 ||
        values[3] > 200 || values[4] > 2 || values[5] > 1 || values[6] > 255 ||
        values[7] < WP_DOOM_FIST || values[7] > WP_DOOM_BFG) {
        return;
    }
    auto &ps = player->client->ps;
    const int ammo[] = {WP_DOOM_PISTOL, WP_DOOM_SHOTGUN, WP_DOOM_ROCKET, WP_DOOM_PLASMA};
    for (int i = 0; i < 4; ++i) {
        if (values[8 + i] > (i == 0 ? 400 : i == 1 ? 100 : i == 2 ? 100 : 600)) {
            return;
        }
    }
    ps.stats[STAT_DOOM_BACKPACK] = values[5];
    player->health = ps.stats[STAT_HEALTH] = values[2];
    ps.stats[STAT_ARMOR] = values[3];
    ps.stats[STAT_DOOM_ARMOR_TYPE] = values[4];
    memset(ps.weapons, 0, sizeof(ps.weapons));
    for (int i = 0; i < 8; ++i) {
        if (values[6] & (1 << i)) {
            COM_BitSet(ps.weapons, WP_DOOM_FIST + i);
        }
    }
    COM_BitSet(ps.weapons, WP_DOOM_FIST);
    COM_BitSet(ps.weapons, WP_DOOM_PISTOL);
    for (int i = 0; i < 4; ++i) {
        ps.ammo[ammo[i]] = min(values[8 + i], doomweapon::Maximum(ps, ammo[i]));
    }
    ps.weapon = doomweapon::Selectable(ps, values[7]) ? values[7] : WP_DOOM_PISTOL;
    G_Printf("Doom inventory carried into %s: health=%d armor=%d weapons=%d bullets=%d\n", map,
             player->health, ps.stats[STAT_ARMOR], values[6], ps.ammo[WP_DOOM_PISTOL]);
}
DOOM_SAVE_CALLBACK("G_DoomApplyCarry", G_DoomApplyCarry);
