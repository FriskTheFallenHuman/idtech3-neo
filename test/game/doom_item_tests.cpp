// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/game/g_local.h"
#include "game/game/bg_doomitems.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

level_locals_t level{};
gentity_t g_entities[MAX_GENTITIES]{};
vmCvar_t g_gametype{};
bool DoomBotsActive() {
    return g_gametype.integer == GT_FFA || g_gametype.integer == GT_TEAM;
}
void DoomItemThink(gentity_t *item);
namespace {
int removed, events, notices;
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace
int G_DoomSkill() {
    return 2;
}
void QDECL G_Printf(const char *, ...) {
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
void G_FreeEntity(gentity_t *entity) {
    entity->inuse = qfalse;
    ++removed;
}
gentity_t *G_Spawn() {
    throw std::runtime_error("Unexpected spawn");
}
void G_SetOrigin(gentity_t *entity, vec3_t origin) {
    VectorCopy(origin, entity->r.currentOrigin);
}
void trap_LinkEntity(gentity_t *) {
}
int G_SoundIndex(const char *) {
    return 1;
}
void G_AddEvent(gentity_t *, int, int) {
    ++events;
}
void trap_SendServerCommand(int, const char *) {
    ++notices;
}

int main() {
    try {
        g_gametype.integer = GT_SINGLE_PLAYER;
        using namespace doomitem;
        playerState_t ps{};
        ps.stats[STAT_HEALTH] = 50;
        doomweapon::Initialize(ps);
        auto take = [&](int thing, int time = 1000) { return Collect(ps, ForThing(thing), time); };
        Check(take(2011) && ps.stats[STAT_HEALTH] == 60, "Stimpack heals ten");
        Check(take(2012) && ps.stats[STAT_HEALTH] == 85, "Medikit heals twenty-five");
        Check(take(2012) && ps.stats[STAT_HEALTH] == 100 && !take(2011),
              "Health cap rejects unneeded medicine");
        Check(take(2014) && ps.stats[STAT_HEALTH] == 101, "Health bonus exceeds one hundred");
        Check(take(2013) && ps.stats[STAT_HEALTH] == 200 && take(2014),
              "Soul and bonuses retain original two-hundred cap/consumption");
        Check(take(2018) && ps.stats[STAT_ARMOR] == 100 && AbsorbDamage(ps, 30) == 10,
              "Green armor absorbs integer one-third damage");
        Check(take(2019) && ps.stats[STAT_ARMOR] == 200 && !take(2018) &&
                  AbsorbDamage(ps, 31) == 15,
              "Blue armor absorbs half and cannot be downgraded at high armor");
        ps.stats[STAT_ARMOR] = 1;
        Check(AbsorbDamage(ps, 100) == 1 && !ps.stats[STAT_DOOM_ARMOR_TYPE] && take(2015) &&
                  ps.stats[STAT_DOOM_ARMOR_TYPE] == 1,
              "Armor exhaustion/bonus restore the correct protection type");
        ps.stats[STAT_HEALTH] = 10;
        Check(take(2023) && ps.stats[STAT_HEALTH] == 100 && ps.stats[STAT_DOOM_BERSERK],
              "Berserk heals and remains active");
        Check(take(2022) && ps.powerups[PW_INVULNERABLE] == 31000 && take(2022, 2000) &&
                  ps.powerups[PW_INVULNERABLE] == 32000,
              "Invulnerability resets to thirty seconds");
        Check(take(2024) && ps.powerups[PW_INVIS] == 61000 && take(2025) &&
                  ps.powerups[PW_BATTLESUIT] == 61000 && take(2045) &&
                  ps.powerups[PW_QUAD] == 121000,
              "Original invisibility, suit and visor durations");
        Check(take(2026) && !take(2026) && ps.stats[STAT_DOOM_MAP],
              "Computer map is permanent per level");
        Check(take(40) && (ps.stats[STAT_KEYS] & (1 << 1)) && (ps.stats[STAT_KEYS] & (1 << 4)) &&
                  !take(40),
              "Skull grants its color and distinct skull HUD bit");
        Check(!Collect(ps, -1, 0) && !Collect(ps, Count, 0), "Invalid pickup indices reject");
        for (int i = 0; i < Count; ++i) {
            std::ifstream input(va("base/models/items/%s/animation.cfg", definitions[i].directory));
            std::string text{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};
            doommonster::SpriteSet set{};
            Check(input && doommonster::Parse(text.data(), set),
                  "Every item animation config parses");
            for (int patch = 0; patch < set.patchCount; ++patch) {
                Check(std::ifstream(std::string("base/") + set.patches[patch].path + ".png").good(),
                      "Every original item frame resolves to an installed PNG");
            }
        }
        gclient_t client{};
        gentity_t player{}, item{};
        level.time = 5000;
        player.client = &client;
        player.health = client.ps.stats[STAT_HEALTH] = 50;
        doomweapon::Initialize(client.ps);
        VectorSet(player.r.mins, -16, -16, -24);
        VectorSet(player.r.maxs, 16, 16, 32);
        player.r.currentOrigin[2] = 24;
        item.s.frame = ForThing(2012);
        item.inuse = qtrue;
        G_DoomItemTouch(&item, &player, nullptr);
        Check(player.health == 75 && client.ps.stats[STAT_HEALTH] == 75 && removed == 1 &&
                  events == 1 && notices == 2,
              "Server touch synchronizes authoritative health and pickup feedback");
        item.r.currentOrigin[2] = -9;
        G_DoomItemTouch(&item, &player, nullptr);
        player.doomNpc.type = doommonster::Imp;
        item.r.currentOrigin[2] = 0;
        G_DoomItemTouch(&item, &player, nullptr);
        Check(removed == 1, "Items below reach and client-backed monsters cannot collect");
        item.doom.sector = 7;
        item.doom.base = 64;
        level.num_entities = MAX_CLIENTS + 1;
        auto &floor = g_entities[MAX_CLIENTS];
        floor.inuse = qtrue;
        floor.classname = "doom_mover";
        floor.doom.sector = 7;
        floor.doom.plane = 0;
        floor.r.currentOrigin[2] = 24;
        DoomItemThink(&item);
        Check(item.r.currentOrigin[2] == 88 && item.nextthink == 5100,
              "Pickup follows its sector floor upward");
        floor.r.currentOrigin[2] = -16;
        DoomItemThink(&item);
        Check(item.r.currentOrigin[2] == 48, "Pickup follows a lowering floor");
        g_gametype.integer = GT_FFA;
        player.doomNpc.type = 0;
        player.health = client.ps.stats[STAT_HEALTH] = 50;
        item.r.currentOrigin[2] = 0;
        item.inuse = qtrue;
        item.s.frame = ForThing(2012);
        G_DoomItemTouch(&item, &player, nullptr);
        Check(item.inuse && (item.s.eFlags & EF_NODRAW) && !item.r.contents &&
                  item.nextthink == 35000 && removed == 1,
              "Deathmatch pickups hide for thirty seconds instead of being freed");
        level.time = item.nextthink;
        item.think(&item);
        Check(!(item.s.eFlags & EF_NODRAW) && item.r.contents == CONTENTS_TRIGGER &&
                  item.think == DoomItemThink,
              "Respawned pickups become visible and touchable again");
        item.count = 1;
        G_DoomItemTouch(&item, &player, nullptr);
        Check(!item.inuse && removed == 2, "Dropped items never respawn");
        std::puts("Doom item health, armor, keys, ammo, powerups, sprite assets and server touch "
                  "checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
