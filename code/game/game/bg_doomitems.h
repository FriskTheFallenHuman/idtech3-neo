// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bg_doomweapons.h"

// Shared pickup rules derived from the original Doom p_inter.cpp. Game owns
// touching/removing entities; cgame owns their sprite rendering and view effects.
namespace doomitem {
enum Kind {
    Weapon,
    Health,
    HealthBonus,
    Armor,
    ArmorBonus,
    Soul,
    Invulnerability,
    Berserk,
    Invisibility,
    Suit,
    Map,
    Light,
    Key
};
struct Definition {
    int thing;
    const char *directory, *name;
    Kind kind;
    int value;
    bool countItem;
};
inline constexpr Definition definitions[] = {
    {2001, "shotgun", "You got the shotgun!", Weapon, 0, false},
    {2002, "chaingun", "You got the chaingun!", Weapon, 1, false},
    {2003, "rocketlauncher", "You got the rocket launcher!", Weapon, 2, false},
    {2004, "plasma", "You got the plasma gun!", Weapon, 3, false},
    {2005, "chainsaw", "A chainsaw! Find some meat!", Weapon, 4, false},
    {2006, "bfg9000", "You got the BFG9000!", Weapon, 5, false},
    {2007, "clip", "Picked up a clip.", Weapon, 6, false},
    {2048, "bulletbox", "Picked up a box of bullets.", Weapon, 7, false},
    {2008, "shells", "Picked up 4 shotgun shells.", Weapon, 8, false},
    {2049, "shellbox", "Picked up a box of shotgun shells.", Weapon, 9, false},
    {2010, "rocket", "Picked up a rocket.", Weapon, 10, false},
    {2046, "rocketbox", "Picked up a box of rockets.", Weapon, 11, false},
    {2047, "cell", "Picked up an energy cell.", Weapon, 12, false},
    {17, "cellpack", "Picked up an energy cell pack.", Weapon, 13, false},
    {8, "backpack", "Picked up a backpack full of ammo!", Weapon, 14, false},
    {2011, "stimpack", "Picked up a stimpack.", Health, 10, false},
    {2012, "medikit", "Picked up a medikit.", Health, 25, false},
    {2014, "healthbonus", "Picked up a health bonus.", HealthBonus, 1, true},
    {2015, "armorbonus", "Picked up an armor bonus.", ArmorBonus, 1, true},
    {2013, "soulsphere", "Supercharge!", Soul, 100, true},
    {2018, "greenarmor", "Picked up the armor.", Armor, 1, false},
    {2019, "bluearmor", "Picked up the MegaArmor!", Armor, 2, false},
    {2022, "invulnerability", "Invulnerability!", Invulnerability, 30000, true},
    {2023, "berserk", "Berserk!", Berserk, 1, true},
    {2024, "invisibility", "Partial Invisibility", Invisibility, 60000, true},
    {2025, "radiationsuit", "Radiation Shielding Suit", Suit, 60000, true},
    {2026, "computermap", "Computer Area Map", Map, 1, true},
    {2045, "lightvisor", "Light Amplification Visor", Light, 120000, true},
    {5, "bluekey", "Picked up a blue keycard.", Key, 1, false},
    {6, "yellowkey", "Picked up a yellow keycard.", Key, 2, false},
    {13, "redkey", "Picked up a red keycard.", Key, 3, false},
    {40, "blueskull", "Picked up a blue skull key.", Key, 4, false},
    {39, "yellowskull", "Picked up a yellow skull key.", Key, 5, false},
    {38, "redskull", "Picked up a red skull key.", Key, 6, false},
};
inline constexpr int Count = sizeof(definitions) / sizeof(definitions[0]);
inline int ForThing(int thing) {
    for (int i = 0; i < Count; ++i) {
        if (definitions[i].thing == thing) {
            return i;
        }
    }
    return -1;
}
inline bool Collect(playerState_t &ps, int index, int time, bool doubleAmmo = false,
                    bool dropped = false) {
    if (index < 0 || index >= Count || ps.stats[STAT_HEALTH] <= 0) {
        return false;
    }
    const auto &item = definitions[index];
    int &health = ps.stats[STAT_HEALTH];
    int &armor = ps.stats[STAT_ARMOR];
    switch (item.kind) {
    case Weapon:
        if (!doomweapon::Collect(ps, item.value, doubleAmmo, dropped)) {
            return false;
        }
        break;
    case Health:
        if (health >= 100) {
            return false;
        }
        health = min(100, health + item.value);
        break;
    case HealthBonus:
        health = min(200, health + 1);
        break;
    case ArmorBonus:
        armor = min(200, armor + 1);
        if (!ps.stats[STAT_DOOM_ARMOR_TYPE]) {
            ps.stats[STAT_DOOM_ARMOR_TYPE] = 1;
        }
        break;
    case Armor:
        if (armor >= item.value * 100) {
            return false;
        }
        armor = item.value * 100;
        ps.stats[STAT_DOOM_ARMOR_TYPE] = item.value;
        break;
    case Soul:
        health = min(200, health + 100);
        break;
    case Invulnerability:
        ps.powerups[PW_INVULNERABLE] = time + item.value;
        break;
    case Berserk:
        health = max(100, health);
        ps.stats[STAT_DOOM_BERSERK] = 1;
        break;
    case Invisibility:
        ps.powerups[PW_INVIS] = time + item.value;
        break;
    case Suit:
        ps.powerups[PW_BATTLESUIT] = time + item.value;
        break;
    case Map:
        if (ps.stats[STAT_DOOM_MAP]) {
            return false;
        }
        ps.stats[STAT_DOOM_MAP] = 1;
        break;
    case Light:
        ps.powerups[PW_QUAD] = time + item.value;
        break;
    case Key:
        if (ps.stats[STAT_KEYS] & (1 << item.value)) {
            return false;
        }
        ps.stats[STAT_KEYS] |= 1 << item.value;
        // Existing Doom line adapters use color bits 1..3 for cards and skulls.
        if (item.value >= 4) {
            ps.stats[STAT_KEYS] |= 1 << (item.value - 3);
        }
        break;
    }
    if (item.countItem) {
        ++ps.stats[STAT_DOOM_ITEMS];
    }
    return true;
}
inline int AbsorbDamage(playerState_t &ps, int damage) {
    const int type = ps.stats[STAT_DOOM_ARMOR_TYPE];
    int saved = type == 1 ? damage / 3 : type == 2 ? damage / 2 : 0;
    saved = min(ps.stats[STAT_ARMOR], saved);
    ps.stats[STAT_ARMOR] -= saved;
    if (!ps.stats[STAT_ARMOR]) {
        ps.stats[STAT_DOOM_ARMOR_TYPE] = 0;
    }
    return saved;
}
} // namespace doomitem
