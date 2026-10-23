// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Include after bg_public.h. These rules run identically in prediction and game.
namespace doomweapon {
constexpr int ImpactBfgSpray = 1; // entityState_t::dmgFlags on secondary spray contacts.
struct Definition {
    const char *directory;
    const char *name;
    const char *sound;
    int ammo, maximum, cost, cycleTics, fireTic, tailTics;
};
inline constexpr Definition definitions[] = {
    {"fist", "Fist", "punch", 0, 0, 0, 17, 4, 5},
    {"pistol", "Pistol", "pistol", WP_DOOM_PISTOL, 200, 1, 14, 4, 5},
    {"shotgun", "Shotgun", "shotgn", WP_DOOM_SHOTGUN, 50, 1, 37, 3, 7},
    {"chaingun", "Chaingun", "pistol", WP_DOOM_PISTOL, 200, 1, 4, 0, 0},
    {"rocketlauncher", "Rocket launcher", "rlaunc", WP_DOOM_ROCKET, 50, 1, 20, 8, 0},
    {"chainsaw", "Chainsaw", "sawful", 0, 0, 0, 4, 0, 0},
    {"plasma", "Plasma rifle", "plasma", WP_DOOM_PLASMA, 300, 1, 3, 0, 20},
    {"bfg9000", "BFG9000", "bfg", WP_DOOM_PLASMA, 300, 40, 40, 30, 20},
};
inline bool IsWeapon(int weapon) {
    return weapon >= WP_DOOM_FIST && weapon <= WP_DOOM_BFG;
}
inline const Definition *Get(int weapon) {
    return IsWeapon(weapon) ? &definitions[weapon - WP_DOOM_FIST] : nullptr;
}
inline int Milliseconds(int tics) {
    return (tics * 1000 + 17) / 35;
}
inline int Maximum(const playerState_t &ps, int weapon) {
    const auto *def = Get(weapon);
    return def ? def->maximum * (ps.stats[STAT_DOOM_BACKPACK] ? 2 : 1) : 0;
}
inline bool Owned(const playerState_t &ps, int weapon) {
    return IsWeapon(weapon) &&
           (static_cast<unsigned>(ps.weapons[weapon / 32]) & (1u << (weapon % 32)));
}
inline bool Selectable(const playerState_t &ps, int weapon) {
    const auto *def = Get(weapon);
    return def && Owned(ps, weapon) && (!def->cost || ps.ammo[def->ammo] >= def->cost);
}
inline int Fallback(const playerState_t &ps) {
    const int preference[] = {WP_DOOM_PLASMA,   WP_DOOM_CHAINGUN, WP_DOOM_SHOTGUN, WP_DOOM_PISTOL,
                              WP_DOOM_CHAINSAW, WP_DOOM_ROCKET,   WP_DOOM_BFG,     WP_DOOM_FIST};
    for (int weapon : preference) {
        if (Selectable(ps, weapon)) {
            return weapon;
        }
    }
    return WP_DOOM_FIST;
}
inline void Animate(playerState_t &ps, int animation, bool restart = true) {
    if (restart || (ps.weapAnim & ~ANIM_TOGGLEBIT) != animation) {
        ps.weapAnim = ((ps.weapAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | animation;
    }
}
inline void Initialize(playerState_t &ps) {
    memset(ps.weapons, 0, sizeof(ps.weapons));
    memset(ps.ammo, 0, sizeof(ps.ammo));
    memset(ps.ammoclip, 0, sizeof(ps.ammoclip));
    ps.stats[STAT_DOOM_WEAPONS] = 1;
    ps.stats[STAT_DOOM_BACKPACK] = 0;
    ps.stats[STAT_DOOM_REFIRE] = 0;
    ps.stats[STAT_DOOM_ARMOR_TYPE] = 0;
    ps.stats[STAT_DOOM_BERSERK] = 0;
    ps.stats[STAT_DOOM_MAP] = 0;
    ps.stats[STAT_DOOM_ITEMS] = 0;
    ps.weapons[WP_DOOM_FIST / 32] |= (1u << (WP_DOOM_FIST % 32));
    ps.weapons[WP_DOOM_PISTOL / 32] |= (1u << (WP_DOOM_PISTOL % 32));
    ps.ammo[WP_DOOM_PISTOL] = 50;
    ps.weapon = WP_DOOM_PISTOL;
    ps.weaponstate = WEAPON_RAISING;
    ps.weaponTime = Milliseconds(16);
    ps.weaponDelay = ps.weapAnimTimer = 0;
    ps.releasedFire = qtrue;
    Animate(ps, WEAP_RAISE);
}
inline bool AddAmmo(playerState_t &ps, int weapon, int amount) {
    const auto *def = Get(weapon);
    if (!def || !def->cost || amount <= 0) {
        return false;
    }
    const int maximum = Maximum(ps, weapon);
    const int current = ps.ammo[def->ammo];
    if (current >= maximum) {
        return false;
    }
    ps.ammo[def->ammo] = current + (amount < maximum - current ? amount : maximum - current);
    return true;
}
struct Pickup {
    int thing, weapon, amount;
    const char *sprite;
    const char *name;
    bool grantsWeapon;
};
inline constexpr Pickup pickups[] = {
    {2001, WP_DOOM_SHOTGUN, 8, "shotgun/shota0", "Shotgun", true},
    {2002, WP_DOOM_CHAINGUN, 20, "chaingun/mguna0", "Chaingun", true},
    {2003, WP_DOOM_ROCKET, 2, "rocketlauncher/launa0", "Rocket launcher", true},
    {2004, WP_DOOM_PLASMA, 40, "plasma/plasa0", "Plasma rifle", true},
    {2005, WP_DOOM_CHAINSAW, 0, "chainsaw/csawa0", "Chainsaw", true},
    {2006, WP_DOOM_BFG, 40, "bfg9000/bfuga0", "BFG9000", true},
    {2007, WP_DOOM_PISTOL, 10, "ammo/clipa0", "Clip", false},
    {2048, WP_DOOM_PISTOL, 50, "ammo/ammoa0", "Box of bullets", false},
    {2008, WP_DOOM_SHOTGUN, 4, "ammo/shela0", "Shells", false},
    {2049, WP_DOOM_SHOTGUN, 20, "ammo/sboxa0", "Box of shells", false},
    {2010, WP_DOOM_ROCKET, 1, "ammo/rocka0", "Rocket", false},
    {2046, WP_DOOM_ROCKET, 5, "ammo/broka0", "Box of rockets", false},
    {2047, WP_DOOM_PLASMA, 20, "ammo/cella0", "Energy cell", false},
    {17, WP_DOOM_PLASMA, 100, "ammo/celpa0", "Energy cell pack", false},
    {8, 0, 0, "ammo/bpaka0", "Backpack", false},
};
inline constexpr int PickupCount = sizeof(pickups) / sizeof(pickups[0]);
inline bool Collect(playerState_t &ps, int index, bool doubleAmmo = false, bool dropped = false) {
    if (index < 0 || index >= PickupCount) {
        return false;
    }
    const auto &pickup = pickups[index];
    const int multiplier = doubleAmmo ? 2 : 1;
    if (pickup.thing == 8) {
        bool changed = !ps.stats[STAT_DOOM_BACKPACK];
        ps.stats[STAT_DOOM_BACKPACK] = 1;
        changed |= AddAmmo(ps, WP_DOOM_PISTOL, 10 * multiplier);
        changed |= AddAmmo(ps, WP_DOOM_SHOTGUN, 4 * multiplier);
        changed |= AddAmmo(ps, WP_DOOM_ROCKET, multiplier);
        changed |= AddAmmo(ps, WP_DOOM_PLASMA, 20 * multiplier);
        return changed;
    }
    const bool acquired = pickup.grantsWeapon && !Owned(ps, pickup.weapon);
    const bool added =
        AddAmmo(ps, pickup.weapon, (dropped ? pickup.amount / 2 : pickup.amount) * multiplier);
    if (acquired) {
        ps.weapons[pickup.weapon / 32] |= 1u << (pickup.weapon % 32);
    }
    return acquired || added;
}
// Return a fire event; animation transitions use the existing replicated weapAnim
// and ANIM_TOGGLEBIT. Ammo is consumed at the action frame, including BFG windup.
inline int Step(playerState_t &ps, const usercmd_t &cmd, int msec) {
    ps.eFlags &= ~EF_FIRING;
    if (!(cmd.buttons & BUTTON_ATTACK)) {
        ps.releasedFire = qtrue;
    }
    if (ps.stats[STAT_HEALTH] <= 0 || ps.pm_type >= PM_DEAD ||
        ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        ps.weaponTime = ps.weaponDelay = 0;
        ps.weaponstate = WEAPON_READY;
        return 0;
    }
    if (ps.pm_flags & PMF_RESPAWNED) {
        return 0;
    }
    if (!IsWeapon(ps.weapon)) {
        ps.weapon = Fallback(ps);
        ps.weaponTime = ps.weaponDelay = 0;
        ps.weaponstate = WEAPON_READY;
        Animate(ps, WEAP_IDLE1);
    }
    const auto *def = Get(ps.weapon);
    const bool wasBusy = ps.weaponTime > 0;
    if (wasBusy) {
        ps.weaponTime -= msec;
    }
    if (ps.weaponDelay > 0) {
        ps.weaponDelay -= msec;
        if (ps.weaponDelay <= 0) {
            ps.weaponDelay = 0;
            if (!def->cost || ps.ammo[def->ammo] >= def->cost) {
                ps.ammo[def->ammo] -= def->cost;
                ps.eFlags |= EF_FIRING;
                return EV_FIRE_WEAPON;
            }
        }
    }
    if (ps.weaponTime > 0) {
        return 0;
    }
    if (ps.weaponstate == WEAPON_DROPPING) {
        ps.weapon = Selectable(ps, cmd.weapon) ? cmd.weapon : Fallback(ps);
        ps.weaponstate = WEAPON_RAISING;
        ps.weaponTime += Milliseconds(16);
        Animate(ps, WEAP_RAISE);
        return 0;
    }
    if (ps.weaponstate == WEAPON_RAISING) {
        ps.weaponstate = WEAPON_READY;
        Animate(ps, WEAP_IDLE1);
    }
    const int desired = Selectable(ps, cmd.weapon) ? cmd.weapon : ps.weapon;
    if (desired != ps.weapon || !Selectable(ps, ps.weapon)) {
        ps.weaponstate = WEAPON_DROPPING;
        ps.weaponTime = Milliseconds(16);
        ps.weaponDelay = 0;
        Animate(ps, WEAP_DROP);
        return 0;
    }
    if (!(cmd.buttons & BUTTON_ATTACK)) {
        if (ps.weaponstate == WEAPON_FIRING && def->tailTics) {
            ps.weaponTime = Milliseconds(def->tailTics);
            ps.weaponstate = WEAPON_READY;
            // Plasma's release sequence has its own row in weapon.cfg.
            if (ps.weapon == WP_DOOM_PLASMA) {
                Animate(ps, WEAP_ATTACK2);
            }
        } else {
            ps.weaponstate = WEAPON_READY;
            Animate(ps, WEAP_IDLE1, false);
        }
        return 0;
    }
    const bool repeat = ps.weaponstate == WEAPON_FIRING && !ps.releasedFire;
    ps.stats[STAT_DOOM_REFIRE] = repeat ? 1 : 0;
    ps.releasedFire = qfalse;
    const int previous = ps.weapAnim & ~ANIM_TOGGLEBIT;
    const bool alternate = (ps.weapon == WP_DOOM_CHAINGUN || ps.weapon == WP_DOOM_CHAINSAW) &&
                           repeat && previous == WEAP_ATTACK1;
    Animate(ps, alternate ? WEAP_ATTACK2 : WEAP_ATTACK1);
    ps.weaponstate = WEAPON_FIRING;
    // Carry fractional command overrun through automatic fire rather than adding
    // one input frame of latency to every shot.
    ps.weaponTime = (wasBusy ? ps.weaponTime : 0) + Milliseconds(def->cycleTics);
    ps.weaponDelay = Milliseconds(def->fireTic);
    if (ps.weaponDelay) {
        return 0;
    }
    ps.ammo[def->ammo] -= def->cost;
    ps.eFlags |= EF_FIRING;
    return EV_FIRE_WEAPON;
}
} // namespace doomweapon
