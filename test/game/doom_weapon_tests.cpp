// SPDX-License-Identifier: GPL-3.0-or-later
#include "../q_shared.h"
#include "../bg_public.h"
#include "../bg_doomweapons.h"
#include <cstdio>
#include <stdexcept>

namespace {
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
playerState_t Player(int weapon) {
    playerState_t ps{};
    doomweapon::Initialize(ps);
    ps.stats[STAT_HEALTH] = 100;
    ps.weapon = weapon;
    ps.weapons[weapon / 32] |= 1u << (weapon % 32);
    ps.weaponTime = ps.weaponDelay = 0;
    ps.weaponstate = WEAPON_READY;
    ps.ammo[WP_DOOM_PISTOL] = 200;
    ps.ammo[WP_DOOM_SHOTGUN] = 50;
    ps.ammo[WP_DOOM_ROCKET] = 50;
    ps.ammo[WP_DOOM_PLASMA] = 300;
    return ps;
}
int FireFor(playerState_t &ps, int duration, int step) {
    usercmd_t cmd{};
    cmd.weapon = static_cast<byte>(ps.weapon);
    cmd.buttons = BUTTON_ATTACK;
    int shots = 0;
    for (int time = 0; time < duration; time += step) {
        shots += doomweapon::Step(ps, cmd, min(step, duration - time)) == EV_FIRE_WEAPON;
    }
    return shots;
}
} // namespace

int main() {
    try {
        using namespace doomweapon;
        playerState_t initial{};
        Initialize(initial);
        Check(Owned(initial, WP_DOOM_FIST) && Owned(initial, WP_DOOM_PISTOL), "Starting ownership");
        Check(!Owned(initial, WP_DOOM_SHOTGUN) && initial.weapons[0] == 0, "No legacy loadout");
        Check(initial.ammo[WP_DOOM_PISTOL] == 50 && initial.ammoclip[WP_DOOM_PISTOL] == 0,
              "Pistol start must be 50 shared bullets and no magazine");
        Check(!Get(-1) && !Get(WP_NUM_WEAPONS) && !Selectable(initial, 200), "Invalid IDs");
        for (int weapon = WP_DOOM_FIST; weapon <= WP_DOOM_BFG; ++weapon) {
            auto ps = Player(weapon);
            const auto &def = *Get(weapon);
            const int before = ps.ammo[def.ammo];
            Check(FireFor(ps, Milliseconds(def.fireTic) + 2, 1) == 1,
                  "Action frame must fire once");
            Check(before - ps.ammo[def.ammo] == def.cost, "Correct ammo cost at action frame");
            Check((ps.weapAnim & ~ANIM_TOGGLEBIT) == WEAP_ATTACK1, "Existing attack animation");
            Check(ps.weaponDelay == 0 && ps.weaponstate == WEAPON_FIRING, "Delay completes once");
        }
        auto bfg = Player(WP_DOOM_BFG);
        bfg.ammo[WP_DOOM_PLASMA] = 39;
        FireFor(bfg, 1500, 8);
        Check(bfg.weapon != WP_DOOM_BFG && bfg.ammo[WP_DOOM_PLASMA] == 39,
              "BFG must reject fewer than forty cells and select an available weapon");
        bfg = Player(WP_DOOM_BFG);
        Check(FireFor(bfg, 800, 8) == 0 && bfg.ammo[WP_DOOM_PLASMA] == 300,
              "BFG windup must not consume/fire early");
        Check(FireFor(bfg, 80, 8) == 1 && bfg.ammo[WP_DOOM_PLASMA] == 260,
              "BFG fires forty cells after windup");
        auto pistol = Player(WP_DOOM_PISTOL);
        usercmd_t cmd{};
        cmd.weapon = WP_DOOM_SHOTGUN;
        cmd.buttons = BUTTON_ATTACK;
        Step(pistol, cmd, 8);
        Check(pistol.weapon == WP_DOOM_PISTOL, "Cannot select unowned shotgun");
        pistol = Player(WP_DOOM_PISTOL);
        Collect(pistol, 0);
        Step(pistol, cmd, 8);
        Check(pistol.weaponstate == WEAPON_DROPPING &&
                  (pistol.weapAnim & ~ANIM_TOGGLEBIT) == WEAP_DROP,
              "Switch uses existing drop state and animation");
        cmd.buttons = 0;
        for (int time = 0; time < 950; time += 8) {
            Step(pistol, cmd, 8);
        }
        Check(pistol.weapon == WP_DOOM_SHOTGUN && pistol.weaponstate == WEAPON_READY,
              "Switch finishes raising selected weapon");
        auto pickup = Player(WP_DOOM_PISTOL);
        pickup.ammo[WP_DOOM_PISTOL] = 190;
        Check(Collect(pickup, 7) && pickup.ammo[WP_DOOM_PISTOL] == 200, "Ammo cap");
        Check(!Collect(pickup, 7) && !Collect(pickup, -1) && !Collect(pickup, PickupCount),
              "Full ammo and malformed pickup IDs");
        Check(Collect(pickup, 14) && Maximum(pickup, WP_DOOM_PISTOL) == 400 &&
                  Maximum(pickup, WP_DOOM_BFG) == 600,
              "Backpack doubles all ammo limits");
        pickup = Player(WP_DOOM_FIST);
        pickup.ammo[WP_DOOM_PISTOL] = 0;
        Check(Collect(pickup, 6, true) && pickup.ammo[WP_DOOM_PISTOL] == 20, "Easy/nightmare ammo");
        Check(Collect(pickup, 3) && pickup.ammo[WP_DOOM_PLASMA] == 300,
              "New weapon even with full ammo");
        for (int step : {8, 16, 33}) {
            auto plasma = Player(WP_DOOM_PLASMA);
            const int shots = FireFor(plasma, 3000, step);
            Check(shots >= 35 && shots <= 36, "Automatic cadence independent of input frame size");
            Check(plasma.ammo[WP_DOOM_PLASMA] == 300 - shots, "One cell per plasma shot");
        }
        auto dead = Player(WP_DOOM_ROCKET);
        dead.stats[STAT_HEALTH] = 0;
        Check(FireFor(dead, 1000, 8) == 0, "Dead player cannot fire");
        auto frozen = Player(WP_DOOM_PISTOL);
        frozen.pm_flags |= PMF_RESPAWNED;
        Check(FireFor(frozen, 1000, 8) == 0, "Respawn protection blocks held attack");
        auto reloaded = Player(WP_DOOM_PISTOL);
        cmd = {};
        cmd.weapon = WP_DOOM_PISTOL;
        cmd.wbuttons = WBUTTON_RELOAD;
        Step(reloaded, cmd, 8);
        Check(reloaded.weaponstate == WEAPON_READY && reloaded.ammo[WP_DOOM_PISTOL] == 200,
              "Doom weapons have no magazine/reload");
        puts("Doom weapon checks passed: roster, ammo, pickups, action frames, windup, switching, "
             "cadence, invalid IDs, death and respawn.");
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
