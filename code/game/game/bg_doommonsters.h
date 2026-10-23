// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Pointer-free animation data is parsed identically by game and cgame. Network
// state carries a species, clip and start time; view rotation stays client-side.
namespace doommonster {
constexpr int NetworkBase = 256;
// CM keeps a small clearance from each plane. Leave room for both contacts in
// exact 56-unit Doom passages while retaining original visual/body dimensions.
constexpr float HullInset = 0.125f;
constexpr int MaxPatches = 160;
constexpr int MaxFrames = 32;
constexpr int MaxSteps = 64;
enum Species {
    None,
    Zombie,
    ShotgunGuy,
    Imp,
    Demon,
    Spectre,
    LostSoul,
    Cacodemon,
    Baron,
    Cyberdemon,
    SpiderMastermind,
    Marine,
    ImpFireball,
    CacoFireball,
    BaronFireball,
    DoomRocket,
    TechPillar,
    Barrel,
    SpeciesCount
};
enum Clip { Idle, Chase, Melee, Missile, Pain, Death, Gib, ClipCount };
enum Attack { Bullet, Shotgun, Fireball, Bite, Charge, CacoBall, BaronBall, Rocket, Chaingun };
struct Definition {
    const char *name;
    const char *classname;
    int thing, health, radius, height, speed, painChance, attack, damage, world;
    bool flying;
    const char *seeSound, *painSound, *deathSound, *attackSound;
};
extern const Definition definitions[SpeciesCount];
struct Patch {
    char path[64];
    int width, height, left, top, textureWidth, textureHeight;
};
struct View {
    int patch, flip;
};
struct Step {
    int frame, tics, event, bright;
};
struct Animation {
    int count, loop, duration;
    Step steps[MaxSteps];
};
struct SpriteSet {
    int patchCount, frameCount;
    Patch patches[MaxPatches];
    View views[MaxFrames][8];
    Animation clips[ClipCount];
};
bool Parse(char *text, SpriteSet &result);
int TypeForThing(int thing);
int TypeForClass(const char *classname);
bool SpawnAllowed(int options, int skill, bool multiplayer);
int Rotation(float actorYaw, float viewerYaw);
const Step &Sample(const SpriteSet &set, int clip, int elapsedMsec);
int DurationMsec(const Animation &animation);
bool IsSprite(int density);
} // namespace doommonster
