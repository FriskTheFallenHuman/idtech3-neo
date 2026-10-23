// SPDX-License-Identifier: GPL-3.0-or-later
#include "g_save_callbacks.h"
#include "g_local.h"
#include "bg_doomitems.h"
#include "../qcommon/doom_bsp.h"

void DoomItemThink(gentity_t *item);

void DoomItemRespawn(gentity_t *item) {
    item->s.eFlags &= ~EF_NODRAW;
    item->r.contents = CONTENTS_TRIGGER;
    item->think = DoomItemThink;
    item->nextthink = level.time + 100;
    trap_LinkEntity(item);
}
DOOM_SAVE_CALLBACK("DoomItemRespawn", DoomItemRespawn);

void DoomItemThink(gentity_t *item) {
    for (int i = MAX_CLIENTS; i < level.num_entities; ++i) {
        const auto &mover = g_entities[i];
        if (mover.inuse && mover.classname && !Q_stricmp(mover.classname, "doom_mover") &&
            mover.doom.sector == item->doom.sector && !mover.doom.plane) {
            vec3_t origin;
            VectorCopy(item->r.currentOrigin, origin);
            origin[2] = item->doom.base + mover.r.currentOrigin[2];
            G_SetOrigin(item, origin);
            trap_LinkEntity(item);
            break;
        }
    }
    item->nextthink = level.time + 100;
}
DOOM_SAVE_CALLBACK("DoomItemThink", DoomItemThink);

void G_DoomItemTouch(gentity_t *item, gentity_t *other, trace_t *) {
    if (!other->client || other->doomNpc.type || other->health <= 0 ||
        !other->client->ps.stats[STAT_DOOM_WEAPONS]) {
        return;
    }
    const float delta = item->r.currentOrigin[2] - (other->r.currentOrigin[2] + other->r.mins[2]);
    if (delta < -8 || delta > other->r.maxs[2] - other->r.mins[2]) {
        return;
    }
    auto &ps = other->client->ps;
    const int index = item->s.frame;
    if (index < 0 || index >= doomitem::Count) {
        return;
    }
    const auto &definition = doomitem::definitions[index];
    const bool acquired = definition.kind == doomitem::Weapon && definition.value < 6 &&
                          !doomweapon::Owned(ps, doomweapon::pickups[definition.value].weapon);
    if (!doomitem::Collect(ps, index, level.time,
                           g_gametype.integer == GT_SINGLE_PLAYER &&
                               (G_DoomSkill() == 0 || G_DoomSkill() == 4),
                           item->count != 0)) {
        return;
    }
    other->health = ps.stats[STAT_HEALTH];
    const char *sound = acquired ? "wpnup"
                        : definition.kind >= doomitem::Soul && definition.kind < doomitem::Key
                            ? "getpow"
                            : "itemup";
    G_AddEvent(other, EV_GENERAL_SOUND, G_SoundIndex(va("sound/doom/monsters/%s.wav", sound)));
    trap_SendServerCommand(other->s.number, va("cp \"%s\"", definition.name));
    trap_SendServerCommand(other->s.number, "doompickup");
    if (acquired || definition.kind == doomitem::Berserk) {
        trap_SendServerCommand(
            other->s.number,
            va("doomweapon %d",
               acquired ? doomweapon::pickups[definition.value].weapon : WP_DOOM_FIST));
    }
    if (definition.kind == doomitem::Key && g_gametype.integer != GT_SINGLE_PLAYER) {
        return;
    }
    if (DoomBotsActive() && !item->count) {
        item->s.eFlags |= EF_NODRAW;
        item->r.contents = 0;
        item->think = DoomItemRespawn;
        item->nextthink = level.time + (definition.kind >= doomitem::Soul ? 60000 : 30000);
        trap_LinkEntity(item);
    } else {
        G_FreeEntity(item);
    }
}
DOOM_SAVE_CALLBACK("G_DoomItemTouch", G_DoomItemTouch);

void G_DoomSpawnItemThings(const void *mapData) {
    const auto *header = static_cast<const dbsp::Header *>(mapData);
    const auto *base = static_cast<const byte *>(mapData);
    const auto &lump = header->lumps[dbsp::Things];
    const auto *things = reinterpret_cast<const dbsp::Thing *>(base + lump.offset);
    const auto *sectors =
        reinterpret_cast<const dbsp::Sector *>(base + header->lumps[dbsp::Sectors].offset);
    const int sectorCount = header->lumps[dbsp::Sectors].length / sizeof(dbsp::Sector);
    int spawned = 0, weapons = 0, ammo = 0, health = 0, armor = 0, powerups = 0;
    for (int i = 0; i < lump.length / static_cast<int>(sizeof(dbsp::Thing)); ++i) {
        const auto &thing = things[i];
        const int index = doomitem::ForThing(thing.type);
        if (index < 0 || thing.sector < 0 || thing.sector >= sectorCount ||
            !doommonster::SpawnAllowed(thing.options, G_DoomSkill(),
                                       g_gametype.integer != GT_SINGLE_PLAYER)) {
            continue;
        }
        const auto &definition = doomitem::definitions[index];
        if (DoomBotsActive() && definition.kind == doomitem::Key) {
            continue;
        }
        if (definition.kind == doomitem::Weapon) {
            if (doomweapon::pickups[definition.value].grantsWeapon) {
                ++weapons;
            } else {
                ++ammo;
            }
        } else if (definition.kind == doomitem::Health ||
                   definition.kind == doomitem::HealthBonus) {
            ++health;
        } else if (definition.kind == doomitem::Armor || definition.kind == doomitem::ArmorBonus) {
            ++armor;
        } else {
            ++powerups;
        }
        auto *item = G_Spawn();
        item->classname = "doom_item_pickup";
        item->s.eType = ET_DOOM_PICKUP;
        item->s.frame = index;
        item->s.effect1Time = level.time;
        item->touch = G_DoomItemTouch;
        item->doom.sector = thing.sector;
        item->doom.base = sectors[thing.sector].floor;
        item->think = DoomItemThink;
        item->nextthink = level.time + 100;
        item->r.contents = CONTENTS_TRIGGER;
        VectorSet(item->r.mins, -20, -20, -8);
        VectorSet(item->r.maxs, 20, 20, 32);
        vec3_t origin = {static_cast<float>(thing.x), static_cast<float>(thing.y),
                         static_cast<float>(sectors[thing.sector].floor)};
        G_SetOrigin(item, origin);
        trap_LinkEntity(item);
        ++spawned;
    }
    G_Printf("Doom items: spawned %d original pickups at skill %d\n", spawned, G_DoomSkill());
    if (DoomBotsActive()) {
        G_Printf("Doom deathmatch pickups: weapons=%d ammo=%d health=%d armor=%d powerups=%d\n",
                 weapons, ammo, health, armor, powerups);
    }
}
DOOM_SAVE_CALLBACK("G_DoomSpawnItemThings", G_DoomSpawnItemThings);
