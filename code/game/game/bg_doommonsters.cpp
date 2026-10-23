// SPDX-License-Identifier: GPL-3.0-or-later
#include "q_shared.h"
#include "bg_doommonsters.h"
#include <cmath>
#include <cstdint>
#include <cerrno>

namespace doommonster {
const Definition definitions[SpeciesCount] = {
    {"none", "", 0, 0, 0, 0, 0, 0, Bullet, 0, 0, false, "", "", "", ""},
    {"zombieman", "monster_zombieman", 3004, 20, 20, 56, 70, 200, Bullet, 3, 0, false, "posit1",
     "popain", "podth1", "pistol"},
    {"shotgunguy", "monster_shotgunguy", 9, 30, 20, 56, 93, 170, Shotgun, 3, 0, false, "posit2",
     "popain", "podth2", "shotgn"},
    {"imp", "monster_imp", 3001, 60, 20, 56, 93, 200, Fireball, 3, 0, false, "bgsit1", "popain",
     "bgdth1", "claw"},
    {"demon", "monster_demon", 3002, 150, 30, 56, 175, 180, Bite, 4, 1, false, "sgtsit", "dmpain",
     "sgtdth", "sgtatk"},
    {"spectre", "monster_spectre", 58, 150, 30, 56, 175, 180, Bite, 4, 1, false, "sgtsit", "dmpain",
     "sgtdth", "sgtatk"},
    {"lostsoul", "monster_lostsoul", 3006, 100, 16, 56, 47, 256, Charge, 3, 0, true, "sklatk",
     "dmpain", "firxpl", "sklatk"},
    {"cacodemon", "monster_cacodemon", 3005, 400, 31, 56, 93, 128, CacoBall, 5, 1, true, "cacsit",
     "dmpain", "cacdth", "firsht"},
    {"baronofhell", "monster_baronofhell", 3003, 1000, 24, 64, 93, 50, BaronBall, 8, 1, false,
     "brssit", "dmpain", "brsdth", "claw"},
    {"cyberdemon", "monster_cyberdemon", 16, 4000, 40, 110, 187, 20, Rocket, 20, 2, false, "cybsit",
     "dmpain", "cybdth", "rlaunc"},
    {"spidermastermind", "monster_spidermastermind", 7, 3000, 128, 100, 140, 40, Chaingun, 3, 3,
     false, "spisit", "dmpain", "spidth", "shotgn"},
    {"doomguy", "player", 1, 100, 16, 56, 320, 255, Bullet, 5, 0, false, "", "plpain", "pldeth",
     "pistol"},
    {"impfireball", "", 0, 0, 0, 0, 0, 0, Fireball, 0, 0, false, "", "", "", ""},
    {"cacofireball", "", 0, 0, 0, 0, 0, 0, CacoBall, 0, 0, false, "", "", "", ""},
    {"baronfireball", "", 0, 0, 0, 0, 0, 0, BaronBall, 0, 0, false, "", "", "", ""},
    {"doomrocket", "", 0, 0, 0, 0, 0, 0, Rocket, 0, 0, false, "", "", "", ""},
    {"techpillar", "doom_tech_pillar", 48, 0, 16, 16, 0, 0, Bullet, 0, 0, false, "", "", "", ""},
    {"barrel", "doom_barrel", 2035, 20, 10, 42, 0, 0, Bullet, 0, 0, false, "", "", "barexp", ""}};

namespace {
bool Integer(char **text, int minimum, int maximum, int &result) {
    const char *token = COM_Parse(text);
    char *end = nullptr;
    errno = 0;
    const long value = strtol(token, &end, 10);
    if (!*token || !end || *end || errno == ERANGE || value < minimum || value > maximum) {
        return false;
    }
    result = static_cast<int>(value);
    return true;
}
bool Keyword(char **text, const char *word) {
    return !Q_stricmp(COM_Parse(text), word);
}
} // namespace

bool Parse(char *text, SpriteSet &result) {
    SpriteSet parsed{};
    int version;
    if (!Keyword(&text, "doomActor") || !Integer(&text, 1, 1, version) ||
        !Keyword(&text, "patches") || !Integer(&text, 1, MaxPatches, parsed.patchCount)) {
        return false;
    }
    for (int i = 0; i < parsed.patchCount; ++i) {
        auto &p = parsed.patches[i];
        const char *path = COM_Parse(&text);
        if ((strncmp(path, "models/players/", 15) && strncmp(path, "models/items/", 13)) ||
            strlen(path) >= sizeof(p.path) || strstr(path, "..") || strchr(path, '\\') ||
            strchr(path, ':')) {
            return false;
        }
        Q_strncpyz(p.path, path, sizeof(p.path));
        if (!Integer(&text, 1, 1024, p.width) || !Integer(&text, 1, 1024, p.height) ||
            !Integer(&text, -2048, 2048, p.left) || !Integer(&text, -2048, 2048, p.top) ||
            !Integer(&text, p.width, 1024, p.textureWidth) ||
            !Integer(&text, p.height, 1024, p.textureHeight) ||
            (p.textureWidth & (p.textureWidth - 1)) || (p.textureHeight & (p.textureHeight - 1))) {
            return false;
        }
    }
    if (!Keyword(&text, "frames") || !Integer(&text, 1, MaxFrames, parsed.frameCount)) {
        return false;
    }
    for (int f = 0; f < parsed.frameCount; ++f) {
        for (auto &view : parsed.views[f]) {
            if (!Integer(&text, 0, parsed.patchCount - 1, view.patch) ||
                !Integer(&text, 0, 1, view.flip)) {
                return false;
            }
        }
    }
    if (!Keyword(&text, "animations")) {
        return false;
    }
    for (auto &animation : parsed.clips) {
        if (!Integer(&text, 1, MaxSteps, animation.count) ||
            !Integer(&text, 0, 1, animation.loop)) {
            return false;
        }
        for (int i = 0; i < animation.count; ++i) {
            auto &step = animation.steps[i];
            if (!Integer(&text, 0, parsed.frameCount - 1, step.frame) ||
                !Integer(&text, 1, 3500, step.tics) || !Integer(&text, 0, 1, step.event) ||
                !Integer(&text, 0, 1, step.bright)) {
                return false;
            }
            animation.duration += step.tics;
        }
    }
    if (*COM_Parse(&text)) {
        return false;
    }
    result = parsed;
    return true;
}

int TypeForThing(int thing) {
    if (thing == 2035) {
        return Barrel;
    }
    if (thing == 48) {
        return TechPillar;
    }
    for (int i = Zombie; i < Marine; ++i) {
        if (definitions[i].thing == thing) {
            return i;
        }
    }
    return None;
}
int TypeForClass(const char *classname) {
    if (!classname) {
        return None;
    }
    for (int i = Zombie; i < Marine; ++i) {
        if (!Q_stricmp(classname, definitions[i].classname)) {
            return i;
        }
    }
    return None;
}
bool SpawnAllowed(int options, int skill, bool multiplayer) {
    const int bit = skill <= 1 ? 1 : skill == 2 ? 2 : 4;
    return (options & bit) && (multiplayer || !(options & 16));
}
int Rotation(float actorYaw, float viewerYaw) {
    if (!std::isfinite(actorYaw) || !std::isfinite(viewerYaw)) {
        return 0;
    }
    float angle = std::fmod(viewerYaw - actorYaw + 22.5f, 360.0f);
    if (angle < 0) {
        angle += 360;
    }
    return static_cast<int>(angle / 45.0f) & 7;
}
const Step &Sample(const SpriteSet &set, int clip, int elapsedMsec) {
    if (clip < 0 || clip >= ClipCount) {
        clip = Idle;
    }
    const auto &animation = set.clips[clip];
    int tick = static_cast<int>((static_cast<int64_t>(max(0, elapsedMsec)) * 35) / 1000);
    if (animation.loop && animation.duration > 0) {
        tick %= animation.duration;
    }
    for (int i = 0; i < animation.count; ++i) {
        if (tick < animation.steps[i].tics || i == animation.count - 1) {
            return animation.steps[i];
        }
        tick -= animation.steps[i].tics;
    }
    return animation.steps[0];
}
int DurationMsec(const Animation &animation) {
    return (animation.duration * 1000 + 34) / 35;
}
bool IsSprite(int density) {
    return density > NetworkBase && density < NetworkBase + SpeciesCount;
}
} // namespace doommonster
