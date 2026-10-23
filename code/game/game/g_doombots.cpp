// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from Hexen2 player bots; Doom combat and item rules.
// Doom player lifecycle and input scheduling. AAS is advanced once by
// G_DoomAIBeginFrame; run this controller afterwards, before the entity frame.
#include "g_doombots_local.h"

void G_WriteClientSessionData(gclient_t *client);

doomBot_t doomBots[DOOM_MAX_BOTS];
static vmCvar_t botCount, botSkill;
bool DoomBotsActive() {
    return G_DoomMapActive() && (g_gametype.integer == GT_FFA || g_gametype.integer == GT_TEAM);
}
static int readyTime, nextPopulation, serial;
static qboolean keepingRoster;

unsigned int DoomBotRandom(doomBot_t &bot) {
    bot.randomState = bot.randomState * 1664525u + 1013904223u;
    return bot.randomState;
}
static void StoreRoster() {
    int count = 0;
    for (int i = 0; i < DOOM_MAX_BOTS; ++i) {
        doomBot_t &bot = doomBots[i];
        char info[MAX_INFO_STRING] = "";
        if (bot.name[0]) {
            Info_SetValueForKey(info, "name", bot.name);
            Info_SetValueForKey(info, "team", va("%d", bot.team));
            Info_SetValueForKey(info, "skill", va("%d", bot.skill));
            ++count;
        }
        trap_Cvar_Set(va("doom_botRoster%d", i), info);
    }
    trap_Cvar_Set("doom_botCount", va("%d", count));
    trap_Cvar_Update(&botCount);
}
static int PlayerCount() {
    int count = 0;
    for (int i = 0; i < level.maxclients; ++i) {
        if (!g_entities[i].doomNpc.type && level.clients[i].pers.connected != CON_DISCONNECTED &&
            level.clients[i].sess.sessionTeam != TEAM_SPECTATOR) {
            ++count;
        }
    }
    return count;
}
static int PlayerLimit() {
    // Slot zero remains available for the first human on listen servers.
    return g_maxGameClients.integer > 0 ? min(level.maxclients, g_maxGameClients.integer)
                                        : level.maxclients;
}
static void DropBot(doomBot_t &bot, qboolean keep) {
    int clientNum = bot.clientNum;
    if (clientNum >= 0) {
        keepingRoster = keep;
        trap_DropClient(clientNum, "Player bot removed");
        keepingRoster = qfalse;
        trap_BotFreeClient(clientNum);
    }
    bot.clientNum = -1;
    if (!keep) {
        bot.name[0] = 0;
    }
}
void G_DoomBotDisconnect(int clientNum) {
    for (int i = 0; i < DOOM_MAX_BOTS; ++i) {
        doomBot_t &bot = doomBots[i];
        if (bot.clientNum != clientNum) {
            continue;
        }
        bot.clientNum = -1;
        char info[MAX_INFO_STRING];
        trap_GetUserinfo(clientNum, info, sizeof(info));
        if (!keepingRoster && !atoi(Info_ValueForKey(info, "doom_botKeep"))) {
            bot.name[0] = 0;
            StoreRoster();
        }
        break;
    }
}
void G_DoomBotsMakeRoom() {
    if (!DoomBotsActive() || PlayerCount() < PlayerLimit()) {
        return;
    }
    for (int i = DOOM_MAX_BOTS - 1; i >= 0; --i) {
        if (doomBots[i].clientNum < 0) {
            continue;
        }
        DropBot(doomBots[i], qtrue);
        nextPopulation = level.time + 1500;
        break;
    }
}
static void Configure(doomBot_t &bot, int team, int skill, const char *name) {
    memset(&bot, 0, sizeof(bot));
    bot.clientNum = bot.enemyNum = bot.goalNum = bot.avoidGoalNum = -1;
    bot.team = team;
    bot.skill = static_cast<int>(Com_Clamp(1, 5, skill));
    bot.randomState = ++serial * 7919u + 17;
    Q_strncpyz(bot.name, name, sizeof(bot.name));
}
static qboolean SpawnBot(doomBot_t &bot) {
    if (PlayerCount() >= PlayerLimit()) {
        return qfalse;
    }
    int clientNum = trap_BotAllocateClient();
    if (clientNum < 1) {
        return qfalse;
    }
    gentity_t *ent = &g_entities[clientNum];
    memset(ent, 0, sizeof(*ent));
    ent->s.number = clientNum;
    ent->r.svFlags = SVF_BOT;
    char userinfo[MAX_INFO_STRING] = "";
    Info_SetValueForKey(userinfo, "name", bot.name);
    Info_SetValueForKey(userinfo, "ip", "bot");
    Info_SetValueForKey(userinfo, "skill", va("%d", bot.skill));
    Info_SetValueForKey(userinfo, "model", "doomguy/default");
    Info_SetValueForKey(userinfo, "team",
                        bot.team == TEAM_RED    ? "red"
                        : bot.team == TEAM_BLUE ? "blue"
                                                : "auto");
    Info_SetValueForKey(userinfo, "cg_autoactivate", "1");
    Info_SetValueForKey(userinfo, "cg_predictItems", "1");
    Info_SetValueForKey(userinfo, "handicap", "100");
    Info_SetValueForKey(userinfo, "password", g_password.string);
    trap_SetUserinfo(clientNum, userinfo);
    char *denied = ClientConnect(clientNum, qtrue, qtrue);
    if (denied) {
        G_Printf("Doom bot rejected: %s\n", denied);
        trap_BotFreeClient(clientNum);
        return qfalse;
    }
    bot.clientNum = clientNum;
    if (g_gametype.integer == GT_TEAM) {
        ent->client->sess.sessionTeam = bot.team == TEAM_RED || bot.team == TEAM_BLUE
                                            ? static_cast<team_t>(bot.team)
                                            : PickTeam(clientNum);
    } else {
        ent->client->sess.sessionTeam = TEAM_FREE;
    }
    G_WriteClientSessionData(ent->client);
    ClientUserinfoChanged(clientNum);
    ClientBegin(clientNum);
    bot.spawnCount = ent->client->ps.persistant[PERS_SPAWN_COUNT];
    bot.nextDecision = bot.nextRoute = level.time;
    bot.progressTime = level.time;
    VectorCopy(ent->r.currentOrigin, bot.progressOrigin);
    G_Printf("Doom player bot joined: client=%d name=%s team=%d skill=%d\n", clientNum, bot.name,
             bot.team, bot.skill);
    return qtrue;
}
void G_DoomBotsInit() {
    trap_Cvar_Register(&botCount, "doom_botCount", "0", CVAR_ARCHIVE);
    trap_Cvar_Register(&botSkill, "doom_botSkill", "3", CVAR_ARCHIVE);
    for (int i = 0; i < DOOM_MAX_BOTS; ++i) {
        char info[MAX_INFO_STRING];
        trap_Cvar_Register(NULL, va("doom_botRoster%d", i), "", CVAR_ROM);
        trap_Cvar_VariableStringBuffer(va("doom_botRoster%d", i), info, sizeof(info));
        char name[32];
        Q_strncpyz(name, Info_ValueForKey(info, "name"), sizeof(name));
        int team = atoi(Info_ValueForKey(info, "team"));
        Configure(doomBots[i], team == TEAM_RED || team == TEAM_BLUE ? team : TEAM_FREE,
                  atoi(Info_ValueForKey(info, "skill")), name);
    }
    readyTime = nextPopulation = level.time + 600;
    keepingRoster = qfalse;
}
void G_DoomBotsShutdown() {
    // No disconnect effects during shutdown. Private roster cvars survive DLL
    // reloads; freed server slots prevent stale bot reconnects and NPC confusion.
    for (int i = 0; i < DOOM_MAX_BOTS; ++i) {
        if (doomBots[i].clientNum >= 0) {
            trap_BotFreeClient(doomBots[i].clientNum);
        }
        doomBots[i].clientNum = -1;
    }
}
static void Population() {
    trap_Cvar_Update(&botCount);
    trap_Cvar_Update(&botSkill);
    int wanted = static_cast<int>(Com_Clamp(0, DOOM_MAX_BOTS, botCount.integer));
    if (wanted != botCount.integer) {
        trap_Cvar_Set("doom_botCount", va("%d", wanted));
    }
    int count = 0;
    for (int i = 0; i < DOOM_MAX_BOTS; ++i) {
        if (doomBots[i].name[0]) {
            ++count;
        }
    }
    if (count != wanted) {
        for (int i = DOOM_MAX_BOTS - 1; i >= 0 && count > wanted; --i) {
            if (doomBots[i].name[0]) {
                DropBot(doomBots[i], qfalse);
                --count;
            }
        }
        for (int i = 0; i < DOOM_MAX_BOTS && count < wanted; ++i) {
            if (doomBots[i].name[0]) {
                continue;
            }
            Configure(doomBots[i], TEAM_FREE, botSkill.integer, va("Marine%d", i + 1));
            ++count;
        }
        StoreRoster();
    }
    for (int i = DOOM_MAX_BOTS - 1; i >= 0 && PlayerCount() > PlayerLimit(); --i) {
        if (doomBots[i].clientNum >= 0) {
            DropBot(doomBots[i], qtrue);
        }
    }
    for (int i = 0; i < DOOM_MAX_BOTS; ++i) {
        if (doomBots[i].name[0] && doomBots[i].clientNum < 0) {
            SpawnBot(doomBots[i]);
        }
    }
}
void G_DoomBotsFrame() {
    if (!G_DoomMapActive() || !DoomBotsActive() || level.time < readyTime) {
        return;
    }
    if (level.time >= nextPopulation) {
        Population();
        nextPopulation = level.time + 1000;
    }
    int msec = static_cast<int>(Com_Clamp(1, 200, level.time - level.previousTime));
    for (int i = 0; i < DOOM_MAX_BOTS; ++i) {
        doomBot_t &bot = doomBots[i];
        if (bot.clientNum < 0) {
            continue;
        }
        gentity_t *self = &g_entities[bot.clientNum];
        if (!self->inuse || self->client->pers.connected != CON_CONNECTED) {
            continue;
        }
        char message[MAX_STRING_CHARS];
        while (trap_BotGetServerCommand(bot.clientNum, message, sizeof(message))) {
        }
        usercmd_t cmd = {};
        cmd.serverTime = level.time;
        cmd.weapon = bot.visible ? self->client->ps.weapon : doomweapon::Fallback(self->client->ps);
        for (int a = 0; a < 3; ++a) {
            cmd.angles[a] =
                ANGLE2SHORT(self->client->ps.viewangles[a]) - self->client->ps.delta_angles[a];
        }
        self->client->inactivityTime = level.time + 60000;
        if (self->health <= 0) {
            bot.enemyNum = -1;
            bot.hasGoal = qfalse;
            // Bots own their respawn decision on the server. Use the ordinary
            // player spawn path without depending on dead-player input processing.
            if (!level.intermissiontime && level.time > self->client->respawnTime + 500) {
                respawn(self);
                cmd.weapon = self->client->ps.weapon;
                for (int a = 0; a < 3; ++a) {
                    cmd.angles[a] = ANGLE2SHORT(self->client->ps.viewangles[a]) -
                                    self->client->ps.delta_angles[a];
                }
            }
        } else if (!level.intermissiontime) {
            int spawn = self->client->ps.persistant[PERS_SPAWN_COUNT];
            if (spawn != bot.spawnCount) {
                bot.spawnCount = spawn;
                bot.enemyNum = -1;
                bot.hasGoal = qfalse;
                bot.escapeUntil = 0;
                bot.nextDecision = bot.nextRoute = level.time;
                VectorCopy(self->r.currentOrigin, bot.progressOrigin);
                bot.progressTime = level.time;
            }
            if (level.time >= bot.nextDecision) {
                bot.nextDecision = level.time + 100;
                DoomBotSense(bot, self);
                DoomBotGoals(bot, self);
            }
            DoomBotNavigate(bot, self, cmd, msec);
            DoomBotCombat(bot, self, cmd, msec);
            // Release attack/jump for one frame after spawning, just like a human.
            if (self->client->ps.pm_flags & PMF_RESPAWNED) {
                cmd.buttons &= ~BUTTON_ATTACK;
                cmd.upmove = 0;
            }
        }
        vec3_t before;
        VectorCopy(self->r.currentOrigin, before);
        trap_BotUserCommand(bot.clientNum, &cmd);
        bot.moved += Distance(before, self->r.currentOrigin);
        ++bot.commands;
    }
}
qboolean G_DoomBotCommand(const char *command) {
    if (!Q_stricmp(command, "botstatus")) {
        int count = 0;
        for (auto &bot : doomBots) {
            if (!bot.name[0]) {
                continue;
            }
            ++count;
            const auto *entity = bot.clientNum >= 0 ? &g_entities[bot.clientNum] : nullptr;
            G_Printf(
                "Doom bot: client=%d name=%s skill=%d team=%d health=%d score=%d spawn=%d "
                "weapon=%d enemy=%d commands=%u routes=%u/%u attacks=%u moved=%.0f recoveries=%u\n",
                bot.clientNum, bot.name, bot.skill,
                entity ? entity->client->sess.sessionTeam : bot.team, entity ? entity->health : 0,
                entity ? entity->client->ps.persistant[PERS_SCORE] : 0, bot.spawnCount,
                entity ? entity->client->ps.weapon : 0, bot.enemyNum, bot.commands, bot.routes,
                bot.routeAttempts, bot.attacks, bot.moved, bot.recoveries);
            if (entity) {
                G_Printf("Doom bot movement: client=%d origin=%.1f,%.1f,%.1f move=%d,%d,%d "
                         "speed=%d pm=%d flags=%d goal=%d point=%.1f,%.1f,%.1f "
                         "target=%d destination=%.1f,%.1f,%.1f travel=%d owned=%d\n",
                         bot.clientNum, entity->r.currentOrigin[0], entity->r.currentOrigin[1],
                         entity->r.currentOrigin[2], entity->client->pers.cmd.forwardmove,
                         entity->client->pers.cmd.rightmove, entity->client->pers.cmd.upmove,
                         entity->client->ps.speed, entity->client->ps.pm_type,
                         entity->client->ps.pm_flags, bot.goalType, bot.route.point[0],
                         bot.route.point[1], bot.route.point[2], bot.goalNum, bot.goal[0],
                         bot.goal[1], bot.goal[2], bot.route.travelType,
                         static_cast<int>(entity->client->ps.weapons[WP_DOOM_FIST / 32]));
            }
        }
        G_Printf("Doom bot roster: %d mode=%d\n", count, g_gametype.integer);
        return qtrue;
    }
    if (Q_stricmp(command, "addbot") && Q_stricmp(command, "removebot")) {
        return qfalse;
    }
    char name[32];
    trap_Argv(1, name, sizeof(name));
    if (!Q_stricmp(command, "removebot")) {
        int removed = 0;
        for (auto &bot : doomBots) {
            if (bot.name[0] && (!Q_stricmp(name, "all") || !Q_stricmp(name, bot.name))) {
                DropBot(bot, qfalse);
                ++removed;
            }
        }
        StoreRoster();
        G_Printf("Removed %d Doom bots. Usage: removebot <name|all>\n", removed);
        return qtrue;
    }
    if (!DoomBotsActive()) {
        G_Printf("Doom bots require a deathmatch or team deathmatch server.\n");
        return qtrue;
    }
    char skillText[16], teamText[16];
    trap_Argv(2, skillText, sizeof(skillText));
    trap_Argv(3, teamText, sizeof(teamText));
    const int skill = skillText[0] ? atoi(skillText) : botSkill.integer;
    const int team = !Q_stricmp(teamText, "red")    ? TEAM_RED
                     : !Q_stricmp(teamText, "blue") ? TEAM_BLUE
                                                    : TEAM_FREE;
    if (!name[0] || skill < 1 || skill > 5 ||
        (skillText[0] && (strlen(skillText) != 1 || skillText[0] < '1' || skillText[0] > '5')) ||
        (teamText[0] && Q_stricmp(teamText, "red") && Q_stricmp(teamText, "blue") &&
         Q_stricmp(teamText, "auto"))) {
        G_Printf("Usage: addbot <name> [skill 1..5] [red|blue|auto]\n");
        return qtrue;
    }
    for (const char *p = name; *p; ++p) {
        if (static_cast<unsigned char>(*p) < 32 || *p == '"' || *p == '\\' || *p == ';') {
            G_Printf("Invalid bot name.\n");
            return qtrue;
        }
    }
    doomBot_t *freeSlot = nullptr;
    for (auto &bot : doomBots) {
        if (!Q_stricmp(name, bot.name)) {
            G_Printf("Bot name already exists.\n");
            return qtrue;
        }
        if (!bot.name[0] && !freeSlot) {
            freeSlot = &bot;
        }
    }
    if (!freeSlot || PlayerCount() >= PlayerLimit()) {
        G_Printf("No Doom bot capacity available.\n");
        return qtrue;
    }
    Configure(*freeSlot, team, skill, name);
    StoreRoster();
    nextPopulation = level.time;
    return qtrue;
}
