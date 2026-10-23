// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Hexen2 player bots; Doom combat and item rules.
#include "g_doombots_local.h"

qboolean DoomBotEnemy(gentity_t *self, gentity_t *other) {
    if (other == self || !other->inuse || !other->client || !other->takedamage ||
        other->health <= 0 || (other->flags & FL_NOTARGET)) {
        return qfalse;
    }
    return !other->doomNpc.type && other->client->pers.connected == CON_CONNECTED &&
                   other->client->sess.sessionTeam != TEAM_SPECTATOR && !OnSameTeam(self, other)
               ? qtrue
               : qfalse;
}
qboolean DoomBotVisible(gentity_t *self, gentity_t *other) {
    vec3_t eye, target;
    VectorCopy(self->r.currentOrigin, eye);
    eye[2] += self->client->ps.viewheight;
    VectorCopy(other->r.currentOrigin, target);
    target[2] += other->client->ps.viewheight * .65f;
    if (!trap_InPVS(eye, target)) {
        return qfalse;
    }
    trace_t trace;
    trap_Trace(&trace, eye, NULL, NULL, target, self->s.number, MASK_SHOT);
    return trace.fraction == 1 || trace.entityNum == other->s.number ? qtrue : qfalse;
}
void DoomBotSense(doomBot_t &bot, gentity_t *self) {
    int selected = -1;
    float best = 1600;
    vec3_t forward;
    AngleVectors(self->client->ps.viewangles, forward, NULL, NULL);
    for (int i = 0; i < level.maxclients; ++i) {
        gentity_t *other = &g_entities[i];
        if (!DoomBotEnemy(self, other)) {
            continue;
        }
        vec3_t direction;
        VectorSubtract(other->r.currentOrigin, self->r.currentOrigin, direction);
        float distance = VectorNormalize(direction);
        // Peripheral vision or close-range awareness; no targeting through walls.
        if (i != bot.enemyNum && distance > 160 && DotProduct(direction, forward) < -.2f) {
            continue;
        }
        if (distance > 1600 || !DoomBotVisible(self, other)) {
            continue;
        }
        float score = distance * (i == bot.enemyNum ? .7f : 1);
        if (score >= best) {
            continue;
        }
        best = score;
        selected = i;
    }
    bot.visible = selected >= 0 ? qtrue : qfalse;
    if (selected >= 0) {
        if (selected != bot.enemyNum) {
            bot.attackReady = level.time + 650 - bot.skill * 100;
        }
        bot.enemyNum = selected;
        bot.lastSeen = level.time;
        VectorCopy(g_entities[selected].r.currentOrigin, bot.lastEnemy);
    } else if (bot.enemyNum >= 0 && (!DoomBotEnemy(self, &g_entities[bot.enemyNum]) ||
                                     level.time - bot.lastSeen > 2500)) {
        bot.enemyNum = -1;
    }
}
static qboolean CanFire(const playerState_t *ps, int weapon) {
    return doomweapon::Selectable(*ps, weapon) ? qtrue : qfalse;
}
static qboolean Melee(const playerState_t *, int weapon) {
    return weapon == WP_DOOM_FIST || weapon == WP_DOOM_CHAINSAW ? qtrue : qfalse;
}
static float ProjectileSpeed(const playerState_t *, int weapon) {
    return weapon == WP_DOOM_ROCKET                            ? 700.0f
           : weapon == WP_DOOM_PLASMA || weapon == WP_DOOM_BFG ? 875.0f
                                                               : 0;
}
static int ChooseWeapon(const playerState_t *ps, float distance) {
    int chosen = WP_DOOM_FIST;
    float best = -1e6f;
    for (int weapon = WP_DOOM_FIST; weapon <= WP_DOOM_BFG; ++weapon) {
        if (!CanFire(ps, weapon)) {
            continue;
        }
        float score = 0;
        switch (weapon) {
        case WP_DOOM_FIST:
            score = distance < 64 ? 30 : -250;
            break;
        case WP_DOOM_CHAINSAW:
            score = distance < 72 ? 110 : -200;
            break;
        case WP_DOOM_PISTOL:
            score = 15;
            break;
        case WP_DOOM_SHOTGUN:
            score = distance < 450 ? 80 : 30;
            break;
        case WP_DOOM_CHAINGUN:
            score = 70;
            break;
        case WP_DOOM_ROCKET:
            score = distance > 180 ? 90 : -300;
            break;
        case WP_DOOM_PLASMA:
            score = 100;
            break;
        case WP_DOOM_BFG:
            score = distance > 160 && distance < 1000 ? 120 : 40;
            break;
        }
        if (weapon == ps->weapon) {
            score += 8;
        }
        if (score > best) {
            chosen = weapon;
            best = score;
        }
    }
    return chosen;
}
void DoomBotCombat(doomBot_t &bot, gentity_t *self, usercmd_t &cmd, int msec) {
    if (bot.enemyNum < 0 || !bot.visible) {
        return;
    }
    gentity_t *enemy = &g_entities[bot.enemyNum];
    if (!DoomBotEnemy(self, enemy) || !DoomBotVisible(self, enemy)) {
        return;
    }
    playerState_t *ps = &self->client->ps;
    vec3_t eye, target, direction, desired;
    VectorCopy(self->r.currentOrigin, eye);
    eye[2] += ps->viewheight - 6;
    VectorCopy(enemy->r.currentOrigin, target);
    target[2] += enemy->client->ps.viewheight * .65f;
    float distance = Distance(eye, target);
    int weapon = ChooseWeapon(ps, distance);
    cmd.weapon = weapon;
    float speed = ProjectileSpeed(ps, weapon);
    if (speed > 0) {
        VectorMA(target, min(.7f, distance / speed) * (bot.skill / 5.0f),
                 enemy->client->ps.velocity, target);
    }
    VectorSubtract(target, eye, direction);
    vectoangles(direction, desired);
    float error = (6 - bot.skill) * .7f;
    desired[YAW] += sinf((level.time + bot.clientNum * 73) * .003f) * error;
    desired[PITCH] += cosf((level.time + bot.clientNum * 97) * .004f) * error * .6f;

    // Preserve world movement when aim turns independently of navigation.
    float oldYaw = SHORT2ANGLE((cmd.angles[YAW] + ps->delta_angles[YAW]) & 65535);
    vec3_t oldView = {0, oldYaw, 0}, forward, right, move;
    AngleVectors(oldView, forward, right, NULL);
    VectorScale(forward, cmd.forwardmove, move);
    VectorMA(move, cmd.rightmove, right, move);
    DoomBotView(self, cmd, desired, msec * (.12f + bot.skill * .04f));
    float yaw = SHORT2ANGLE((cmd.angles[YAW] + ps->delta_angles[YAW]) & 65535);
    vec3_t view = {0, yaw, 0};
    AngleVectors(view, forward, right, NULL);
    cmd.forwardmove = static_cast<signed char>(Com_Clamp(-127, 127, DotProduct(move, forward)));
    cmd.rightmove = static_cast<signed char>(Com_Clamp(-127, 127, DotProduct(move, right)));
    if (level.time >= bot.escapeUntil && !Melee(ps, weapon) && distance < 500 &&
        bot.goalType == DOOM_BOT_ENEMY && self->waterlevel < 2) {
        if (level.time >= bot.strafeUntil) {
            bot.strafeUntil = level.time + 700 + DoomBotRandom(bot) % 700;
            bot.strafeSign = DoomBotRandom(bot) & 1 ? 1 : -1;
        }
        // A combat dodge needs a clear corridor and a safe floor.
        vec3_t dodge;
        VectorScale(right, bot.strafeSign, dodge);
        if (DoomBotSafeStep(self, dodge, qfalse, qfalse)) {
            cmd.rightmove = bot.strafeSign * 90;
            // Backpedalling must pass the same floor and slope checks.
            vec3_t back;
            VectorScale(forward, -1, back);
            cmd.forwardmove = distance < 160 && DoomBotSafeStep(self, back, qfalse, qfalse) ? -60
                              : distance < 300 ? 0
                                               : cmd.forwardmove;
        }
    }
    float pitch = SHORT2ANGLE((cmd.angles[PITCH] + ps->delta_angles[PITCH]) & 65535);
    float tolerance = Melee(ps, weapon) ? 18 : 5;
    if (level.time < bot.attackReady || ps->weapon != weapon || !CanFire(ps, weapon) ||
        (Melee(ps, weapon) && distance > 88) ||
        fabsf(AngleNormalize180(yaw - desired[YAW])) > tolerance ||
        fabsf(AngleNormalize180(pitch - desired[PITCH])) > tolerance) {
        return;
    }
    vec3_t shotView = {pitch, yaw, 0}, end;
    AngleVectors(shotView, forward, NULL, NULL);
    VectorMA(eye, max(distance, 128.0f), forward, end);
    trace_t trace;
    trap_Trace(&trace, eye, NULL, NULL, end, self->s.number, MASK_SHOT);
    // Never shoot through teammates, and keep rocket splash clear of ourselves
    // and friendly players even when friendly fire is disabled.
    if (trace.entityNum >= 0 && trace.entityNum < level.maxclients &&
        OnSameTeam(self, &g_entities[trace.entityNum])) {
        return;
    }
    if (weapon == WP_DOOM_ROCKET) {
        if (Distance(eye, trace.endpos) < 150) {
            return;
        }
        for (int i = 0; i < level.maxclients; ++i) {
            auto *peer = &g_entities[i];
            if (peer != self && peer->inuse && peer->client && peer->health > 0 &&
                OnSameTeam(self, peer) && Distance(peer->r.currentOrigin, trace.endpos) < 150) {
                return;
            }
        }
    }
    if (trace.fraction < 1 && trace.entityNum != enemy->s.number) {
        return;
    }
    cmd.buttons |= BUTTON_ATTACK;
    ++bot.attacks;
}
