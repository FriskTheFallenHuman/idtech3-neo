// SPDX-License-Identifier: GPL-3.0-or-later
#include "../g_local.h"
#include "../g_doomlevel.h"
#include "../bg_doomlevel.h"
#include "../bg_doomweapons.h"
#include <cstdio>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
level_locals_t level{};
gentity_t g_entities[MAX_GENTITIES]{};
vmCvar_t g_gametype{};
doomProgress_t doomProgress{};
namespace {
gclient_t human{};
std::map<std::string, std::string> cvars{{"mapname", "e1m1"}};
std::string state, command;
void Check(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
} // namespace
int G_DoomSkill() {
    return 2;
}
qboolean G_DoomMapActive() {
    return qtrue;
}
gentity_t *G_FindActor(const char *) {
    return &g_entities[0];
}
void QDECL G_Printf(const char *, ...) {
}
void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
void trap_SetConfigstring(int index, const char *text) {
    if (index == CS_DOOM_INTERMISSION) {
        state = text;
    }
}
void trap_Cvar_Set(const char *name, const char *value) {
    cvars[name] = value;
}
void trap_Cvar_VariableStringBuffer(const char *name, char *value, int size) {
    Q_strncpyz(value, cvars[name].c_str(), size);
}
int trap_Cvar_VariableIntegerValue(const char *name) {
    return atoi(cvars[name].c_str());
}
void trap_SendConsoleCommand(int, const char *text) {
    command += text;
}
int main() {
    try {
        using namespace doomlevel;
        int ep, map;
        Check(Next(1, 3, true, ep, map) && ep == 1 && map == 9,
              "Secret exit enters the episode secret map");
        Check(Next(1, 9, false, ep, map) && map == 4 && Next(2, 9, false, ep, map) && map == 6 &&
                  Next(3, 9, false, ep, map) && map == 7,
              "Every secret map returns to the correct mission");
        Check(Next(3, 8, false, ep, map) && ep == 1 && map == 1 && !Next(4, 1, false, ep, map),
              "Episode rollover and invalid maps");
        Check(Par(1, 2) == 75 && Par(3, 9) == 135 && Percent(0, 0) == 0,
              "Original par times and empty counters");
        Summary sentinel{};
        Check(!Parse("", sentinel) && !Parse("1 1 1 1 2 0 0 0 0 0 0 10 30 1 99 0", sentinel) &&
                  !Parse("1 1 1 1 2 9999999999999999999999 0 0 0 0 0 10 30 1 0 0", sentinel),
              "Malformed replicated summaries reject");
        std::ifstream input("base/maps/e1m1.bsp", std::ios::binary | std::ios::ate);
        Check(input.good(), "E1M1 fixture exists");
        std::vector<char> bytes(static_cast<size_t>(input.tellg()));
        input.seekg(0);
        input.read(bytes.data(), bytes.size());
        g_gametype.integer = GT_SINGLE_PLAYER;
        auto &player = g_entities[0];
        player.inuse = qtrue;
        player.client = &human;
        doomweapon::Initialize(human.ps);
        player.health = human.ps.stats[STAT_HEALTH] = 186;
        human.ps.stats[STAT_ARMOR] = 200;
        human.ps.stats[STAT_DOOM_ARMOR_TYPE] = 2;
        human.ps.stats[STAT_DOOM_BACKPACK] = 1;
        human.ps.stats[STAT_DOOM_ITEMS] = 4;
        human.ps.ammo[WP_DOOM_PISTOL] = 37;
        human.ps.ammo[WP_DOOM_SHOTGUN] = 7;
        COM_BitSet(human.ps.weapons, WP_DOOM_SHOTGUN);
        human.ps.weapon = WP_DOOM_SHOTGUN;
        g_entities[1].inuse = qtrue;
        g_entities[1].doomNpc.type = doommonster::Zombie;
        g_entities[2].inuse = qtrue;
        g_entities[2].doomNpc.type = doommonster::LostSoul;
        level.num_entities = MAX_CLIENTS;
        level.time = 31000;
        doomProgress.lastLoadTime = 1000;
        G_DoomLevelInit(bytes.data());
        human.pers.cmd.buttons = BUTTON_ACTIVATE;
        Check(G_DoomBeginIntermission(&player, false) && Parse(state.c_str(), sentinel),
              "Real server starts a replicated intermission");
        Check(
            sentinel.kills == 1 && sentinel.totalKills == 6 && sentinel.items == 4 &&
                sentinel.totalSecrets == 3 && sentinel.seconds == 30 && sentinel.par == 30,
            "Authoritative corpse/item/secret/time statistics match E1M1; lost souls are excluded");
        Check(human.ps.pm_type == PM_INTERMISSION && !player.takedamage,
              "Intermission freezes the human");
        level.time += 600;
        G_DoomLevelFrame();
        Check(Parse(state.c_str(), sentinel) && sentinel.phase == Counting,
              "Held exit key does not skip the screen");
        human.pers.cmd.buttons = 0;
        G_DoomLevelFrame();
        human.pers.cmd.buttons = BUTTON_ATTACK;
        G_DoomLevelFrame();
        Check(Parse(state.c_str(), sentinel) && sentinel.phase == Statistics,
              "First new press completes counters");
        level.time += 500;
        G_DoomLevelFrame();
        Check(command.empty(), "Holding fire does not advance twice");
        human.pers.cmd.buttons = 0;
        G_DoomLevelFrame();
        human.pers.cmd.buttons = BUTTON_ATTACK;
        G_DoomLevelFrame();
        Check(Parse(state.c_str(), sentinel) && sentinel.phase == Entering,
              "Second press shows the destination map");
        level.time += 4100;
        G_DoomLevelFrame();
        Check(command == "spmap e1m2\n", "Next level uses single-player spmap once");
        G_DoomLevelFrame();
        Check(command == "spmap e1m2\n", "Transition is not queued twice");
        cvars["mapname"] = "e1m2";
        human = {};
        doomweapon::Initialize(human.ps);
        player.health = 100;
        G_DoomApplyCarry(&player);
        Check(player.health == 186 && human.ps.stats[STAT_ARMOR] == 200 &&
                  human.ps.stats[STAT_DOOM_BACKPACK] && human.ps.ammo[WP_DOOM_PISTOL] == 37 &&
                  human.ps.ammo[WP_DOOM_SHOTGUN] == 7 && human.ps.weapon == WP_DOOM_SHOTGUN &&
                  !human.ps.stats[STAT_KEYS] && !human.ps.stats[STAT_DOOM_ITEMS] &&
                  cvars["doom_carry"].empty(),
              "Normal level changes carry health/armor/weapons/ammo while resetting keys and level "
              "counters");
        struct Route {
            const char *demo, *map;
            bool secret;
            const char *expected;
        };
        const Route routes[] = {{"1", "e1m1", false, "spmap e1m2\n"},
                                {"1", "e1m3", true, "spmap e1m9\n"},
                                {"1", "e1m9", false, "spmap e1m4\n"},
                                {"0", "e1m8", false, "spmap e2m1\n"},
                                {"1", "e1m8", false, "disconnect\ndoommenu help\n"}};
        for (const auto &route : routes) {
            command.clear();
            cvars["doom_demo"] = route.demo;
            cvars["mapname"] = route.map;
            cvars["doom_carry"] = "old inventory";
            human.pers.cmd.buttons = 0;
            G_DoomLevelInit(bytes.data());
            Check(G_DoomBeginIntermission(&player, route.secret), "Episode route starts");
            level.time += 11000;
            G_DoomLevelFrame();
            level.time += 500;
            human.pers.cmd.buttons = BUTTON_ATTACK;
            G_DoomLevelFrame();
            if (std::string(route.expected).find("disconnect") == 0) {
                Check(Parse(state.c_str(), sentinel) && sentinel.phase == Leaving &&
                          cvars["doom_carry"].empty(),
                      "Demo ending skips the locked episode's entering screen and clears carry");
            } else {
                Check(Parse(state.c_str(), sentinel) && sentinel.phase == Entering,
                      "Allowed destinations retain the entering screen");
                level.time += 4100;
                G_DoomLevelFrame();
            }
            Check(command == route.expected, "Demo and registered episode routes must match");
            G_DoomLevelFrame();
            Check(command == route.expected, "Episode transition must queue only once");
        }
        std::puts(
            "Doom intermission routing, demo ending, statistics, input edges, replicated data and "
            "inventory carry passed.");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
