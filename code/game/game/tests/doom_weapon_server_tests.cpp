// SPDX-License-Identifier: GPL-3.0-or-later
#include "../g_local.h"
void G_DoomItemTouch(gentity_t *, gentity_t *, trace_t *) {
}
void G_DoomSpawnItemThings(const void *) {
}
#include "../bg_doomweapons.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

level_locals_t level{};
gentity_t g_entities[MAX_GENTITIES]{};
vmCvar_t g_gameskill{};
int G_DoomSkill() {
    return g_gameskill.integer;
}
void G_DoomAIHear(int, const vec3_t, float) {
}
namespace {
gclient_t playerClient{};
gclient_t targetClient{};
gentity_t projectile{}, impact{};
int traceCount, damageCount, lastDamage, lineCount, lastMod;
int impactCount, lastImpactEvent, traceFlags, traceContents;
float lastRange;
bool blocked;
bool shrinkOnDamage;
std::vector<int> damages;
void Check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
gentity_t *Player(int weapon) {
    auto *player = &g_entities[0];
    memset(player, 0, sizeof(*player));
    memset(&playerClient, 0, sizeof(playerClient));
    player->inuse = qtrue;
    player->client = &playerClient;
    playerClient.ps.stats[STAT_DOOM_WEAPONS] = 1;
    playerClient.ps.weapon = weapon;
    playerClient.ps.viewheight = 32;
    g_entities[1].takedamage = qtrue;
    g_entities[1].client = &targetClient;
    traceCount = damageCount = lineCount = 0;
    impactCount = traceFlags = traceContents = 0;
    blocked = false;
    shrinkOnDamage = false;
    damages.clear();
    return player;
}
} // namespace
gentity_t *G_Spawn() {
    memset(&projectile, 0, sizeof(projectile));
    return &projectile;
}
gentity_t *G_TempEntity(vec3_t origin, int event) {
    ++impactCount;
    lastImpactEvent = event;
    memset(&impact, 0, sizeof(impact));
    VectorCopy(origin, impact.s.pos.trBase);
    return &impact;
}
void G_ExplodeMissile(gentity_t *) {
}
void trap_LinkEntity(gentity_t *) {
}
void G_SetOrigin(gentity_t *entity, vec3_t origin) {
    VectorCopy(origin, entity->r.currentOrigin);
    VectorCopy(origin, entity->s.pos.trBase);
}
void G_FreeEntity(gentity_t *entity) {
    entity->inuse = qfalse;
}
int G_SoundIndex(const char *) {
    return 1;
}
void G_AddEvent(gentity_t *, int, int) {
}
void trap_SendServerCommand(int, const char *) {
}
void QDECL G_Printf(const char *, ...) {
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
void trap_Trace(trace_t *trace, const vec3_t start, const vec3_t, const vec3_t, const vec3_t end,
                int ignored, int mask) {
    Check(ignored == 0 && mask == MASK_SHOT, "Hitscan ignores shooter and uses solid/actor mask");
    ++traceCount;
    vec3_t delta;
    VectorSubtract(end, start, delta);
    lastRange = VectorLength(delta);
    memset(trace, 0, sizeof(*trace));
    trace->entityNum = blocked ? ENTITYNUM_WORLD : 1;
    trace->fraction = 0.5f;
    trace->surfaceFlags = traceFlags;
    trace->contents = traceContents;
    VectorSet(trace->plane.normal, 0, 0, 1);
    for (int i = 0; i < 3; ++i) {
        trace->endpos[i] = (start[i] + end[i]) * 0.5f;
    }
    vec3_t travel;
    VectorNormalize2(delta, travel);
    VectorMA(trace->endpos, 20, travel, g_entities[1].r.currentOrigin);
    g_entities[1].r.currentOrigin[2] = trace->endpos[2] - 28;
    VectorSet(g_entities[1].r.mins, -20, -20, 0);
    VectorSet(g_entities[1].r.maxs, 20, 20, 56);
}
qboolean G_DoomTraceLine(gentity_t *, const vec3_t, const vec3_t, qboolean shot) {
    Check(shot == qtrue, "Hitscan activates shootable Doom lines");
    ++lineCount;
    return qtrue;
}
void G_Damage(gentity_t *target, gentity_t *, gentity_t *attacker, vec3_t, vec3_t, int damage,
              int flags, int mod) {
    Check(target == &g_entities[1] && attacker == &g_entities[0], "Damage attribution");
    Check(flags == DAMAGE_NO_KNOCKBACK, "Doom hitscan has no host weapon kick");
    ++damageCount;
    lastDamage = damage;
    lastMod = mod;
    damages.push_back(damage);
    if (shrinkOnDamage) {
        target->r.maxs[2] = 8;
    }
}
int main() {
    try {
        for (int weapon :
             {WP_DOOM_FIST, WP_DOOM_CHAINSAW, WP_DOOM_PISTOL, WP_DOOM_SHOTGUN, WP_DOOM_CHAINGUN}) {
            auto *player = Player(weapon);
            Check(G_DoomFireWeapon(player), "Server accepts Doom weapon");
            const int pellets = weapon == WP_DOOM_SHOTGUN ? 7 : 1;
            Check(traceCount == pellets && damageCount == pellets && lineCount == pellets,
                  "Melee/bullets/pellets trace actual collision independently");
            Check(impactCount == pellets && lastImpactEvent == EV_MISSILE_HIT &&
                      impact.s.weapon == weapon && impact.s.otherEntityNum == 1,
                  "Each pellet and melee hit carries its weapon and flesh target");
            vec3_t throughBody;
            VectorSubtract(impact.s.angles, impact.s.pos.trBase, throughBody);
            Check(fabs(VectorLength(impact.s.origin2) - 1) < .001f &&
                      DotProduct(throughBody, impact.s.origin2) > 38,
                  "Flesh impact carries normalized shot travel and a far-side exit");
            const float range = weapon == WP_DOOM_FIST       ? 64.0f
                                : weapon == WP_DOOM_CHAINSAW ? 65.0f
                                                             : 2048.0f;
            Check(fabs(lastRange - range) < 0.01f, "Doom melee and hitscan reach");
            for (int damage : damages) {
                Check(weapon == WP_DOOM_FIST || weapon == WP_DOOM_CHAINSAW
                          ? damage >= 2 && damage <= 20 && damage % 2 == 0
                          : damage >= 5 && damage <= 15 && damage % 5 == 0,
                      "Original damage dice bounds");
            }
            blocked = true;
            damageCount = 0;
            impactCount = 0;
            traceFlags = SURF_METAL | SURF_NOMARKS;
            traceContents = CONTENTS_SLIME;
            G_DoomFireWeapon(player);
            vec3_t hitNormal;
            ByteToDir(impact.s.eventParm, hitNormal);
            Check(damageCount == 0, "World obstruction prevents damage");
            Check(impactCount == pellets && lastImpactEvent == EV_MISSILE_MISS &&
                      impact.s.otherEntityNum == ENTITYNUM_WORLD &&
                      impact.s.effect2Time == traceFlags && impact.s.effect1Time == traceContents &&
                      hitNormal[2] > 0.99f,
                  "World hits preserve high surface bits and contents independently of weapon");
            impactCount = 0;
            traceFlags = SURF_NOIMPACT;
            G_DoomFireWeapon(player);
            Check(impactCount == 0, "No-impact surfaces suppress effects");
        }
        auto *fistPlayer = Player(WP_DOOM_FIST);
        srand(73);
        G_DoomFireWeapon(fistPlayer);
        const int ordinaryFistDamage = lastDamage;
        fistPlayer = Player(WP_DOOM_FIST);
        fistPlayer->client->ps.stats[STAT_DOOM_BERSERK] = 1;
        srand(73);
        G_DoomFireWeapon(fistPlayer);
        Check(lastDamage == ordinaryFistDamage * 10,
              "Berserk multiplies the authoritative fist damage dice by ten");
        auto *propShooter = Player(WP_DOOM_PISTOL);
        g_entities[1].client = nullptr;
        G_DoomFireWeapon(propShooter);
        Check(damageCount == 1 && lastImpactEvent == EV_MISSILE_MISS,
              "Damageable props receive damage without generating flesh blood");
        auto *lethalShooter = Player(WP_DOOM_PISTOL);
        shrinkOnDamage = true;
        G_DoomFireWeapon(lethalShooter);
        Check(g_entities[1].r.maxs[2] == 8 && impact.s.angles[2] == impact.s.pos.trBase[2] &&
                  VectorLength(impact.s.origin2) > .99f,
              "Exit metadata is captured before damage shrinks the corpse hull");
        for (int weapon : {WP_DOOM_ROCKET, WP_DOOM_PLASMA, WP_DOOM_BFG}) {
            auto *player = Player(weapon);
            level.time = 1000;
            Check(G_DoomFireWeapon(player), "Server launches projectile");
            Check(projectile.s.eType == ET_MISSILE && projectile.s.weapon == weapon,
                  "Projectile retains Doom weapon ID for rendering and impact");
            Check(projectile.parent == player && projectile.r.ownerNum == player->s.number &&
                      projectile.clipmask == MASK_MISSILESHOT,
                  "Projectile collision/ownership");
            const float speed = weapon == WP_DOOM_ROCKET ? 700.0f : 875.0f;
            Check(fabs(VectorLength(projectile.s.pos.trDelta) - speed) < 0.01f &&
                      projectile.s.pos.trType == TR_LINEAR,
                  "Doom projectile speed and trajectory");
            Check(projectile.splashDamage == (weapon == WP_DOOM_ROCKET ? 128 : 0) &&
                      projectile.splashRadius == (weapon == WP_DOOM_ROCKET ? 128 : 0),
                  "Rocket splash independent of plasma and BFG spray");
            Check(projectile.think != nullptr && projectile.nextthink > level.time,
                  "Projectiles have bounded lifetime");
        }
        Player(WP_DOOM_BFG);
        projectile.s.weapon = WP_DOOM_BFG;
        projectile.parent = &g_entities[0];
        G_DoomBfgSpray(&projectile);
        Check(traceCount == 40 && damageCount == 40 && lastMod == MOD_BFG_SPLASH,
              "BFG sprays forty collision-tested rays");
        Check(impactCount == 40 && lastImpactEvent == EV_MISSILE_HIT &&
                  impact.s.weapon == WP_DOOM_BFG && impact.s.dmgFlags == doomweapon::ImpactBfgSpray,
              "BFG spray damage generates its own blood impact events");
        for (int damage : damages) {
            Check(damage >= 15 && damage <= 120, "BFG spray sums fifteen 1..8 dice");
        }
        puts("Doom server weapon checks passed: collision, melee range, shotgun pellets, damage "
             "dice, shootable lines, projectile ownership/speed/splash and BFG spray.");
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
