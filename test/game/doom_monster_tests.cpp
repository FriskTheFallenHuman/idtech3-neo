// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/game/q_shared.h"
#include "game/game/bg_doommonsters.h"
#include "game/cgame/cg_doomsprite_geometry.h"
#include "game/game/g_doomsound.h"
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

void QDECL Com_Printf(const char *, ...) {
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}

namespace {
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
std::string Read(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot read " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
} // namespace

int main(int argc, char **argv) {
    try {
        using namespace doommonster;
        {
            int heads[5] = {-1, -1, -1, -1, -1};
            doomsound::Link links[10]{};
            int linkCount = 0;
            auto connect = [&](int first, int second, bool blocking) {
                links[linkCount] = {heads[first], second, blocking};
                heads[first] = linkCount++;
                links[linkCount] = {heads[second], first, blocking};
                heads[second] = linkCount++;
            };
            connect(0, 1, false);
            connect(1, 2, true);
            connect(2, 3, true);
            connect(0, 4, false);
            connect(4, 3, false);
            float floors[5] = {}, ceilings[5] = {128, 128, 128, 128, 0};
            int levels[5], queue[10];
            doomsound::Flood(0, 5, heads, links, floors, ceilings, levels, queue);
            Check(levels[1] == 0 && levels[2] == 1 && levels[3] == 2 && levels[4] == 2,
                  "Noise crosses one sound-block line and stops at closed doors or a second block");
            ceilings[4] = 128;
            doomsound::Flood(0, 5, heads, links, floors, ceilings, levels, queue);
            Check(levels[4] == 0 && levels[3] == 0 && levels[2] == 1,
                  "Opening a door allows noise to take the alternate connected path");
        }
        const std::string base = argc > 1 ? argv[1] : "base";
        for (int species = Zombie; species < SpeciesCount; ++species) {
            const std::string config =
                Read(base + "/models/players/" + definitions[species].name + "/animation.cfg");
            auto text = config;
            SpriteSet set{};
            Check(Parse(text.data(), set), "Imported animation must parse");
            for (int frame = 0; frame < set.frameCount; ++frame) {
                for (int angle = 0; angle < 8; ++angle) {
                    const auto &patch = set.patches[set.views[frame][angle].patch];
                    const auto png = Read(base + "/" + patch.path + ".png");
                    Check(png.size() >= 24 && png.substr(1, 3) == "PNG",
                          "Every view must resolve to a PNG");
                }
            }
            // Truncation and partial configs cannot overwrite a live set.
            for (size_t end : {size_t(0), config.size() / 2, config.size() - 5}) {
                auto invalid = config.substr(0, end);
                SpriteSet preserved{};
                preserved.patchCount = 1234;
                Check(!Parse(invalid.data(), preserved) && preserved.patchCount == 1234,
                      "Malformed config must reject atomically");
            }
            auto trailing = config + "trailing garbage\n";
            Check(!Parse(trailing.data(), set), "Trailing data must reject");
        }
        Check(TypeForThing(3001) == Imp && TypeForThing(58) == Spectre &&
                  TypeForThing(7) == SpiderMastermind && TypeForThing(16) == Cyberdemon &&
                  TypeForThing(48) == TechPillar && TypeForThing(123456) == None,
              "Original Doom thing mapping");
        for (int species = Zombie; species < Marine; ++species) {
            Check(TypeForClass(definitions[species].classname) == species,
                  "Entity classname mapping");
        }
        Check(SpawnAllowed(1, 0, false) && SpawnAllowed(1, 1, false) &&
                  !SpawnAllowed(1, 2, false) && SpawnAllowed(2, 2, false) &&
                  SpawnAllowed(4, 3, false) && SpawnAllowed(4, 4, false) &&
                  !SpawnAllowed(23, 3, false) && SpawnAllowed(23, 3, true),
              "Skill and mode exclusions");
        for (int angle = 0; angle < 8; ++angle) {
            Check(Rotation(0, angle * 45.0f) == angle, "Eight view sectors");
            Check(Rotation(123, 123 + angle * 45.0f) == angle, "Relative actor rotation");
            const float radians = angle * static_cast<float>(M_PI / 4);
            vec3_t viewer = {cos(radians), sin(radians), 0};
            vec3_t cameraRight = {-viewer[1], viewer[0], 0};
            vec3_t cameraLeft, origin = {0, 0, 10};
            VectorScale(cameraRight, -1, cameraLeft);
            Patch patch{};
            patch.width = 30;
            patch.height = 50;
            patch.left = 12;
            patch.top = 50;
            patch.textureWidth = 32;
            patch.textureHeight = 64;
            for (bool flip : {false, true}) {
                polyVert_t vertices[4]{};
                doomsprite::BuildQuad(origin, viewer, cameraLeft, patch, flip, 0, vertices);
                vec3_t edge;
                VectorSubtract(vertices[1].xyz, vertices[0].xyz, edge);
                Check(DotProduct(edge, cameraRight) > 29.9f &&
                          ((vertices[1].st[0] > vertices[0].st[0]) != flip),
                      "Patch UVs must preserve camera left/right for all eight views");
                Check(vertices[0].xyz[2] == 10 && vertices[2].xyz[2] == 60,
                      "Billboard offsets keep the feet and top stable");
            }
        }
        {
            Patch patch{};
            patch.width = 30;
            patch.height = 50;
            patch.top = 45;
            patch.textureWidth = 32;
            patch.textureHeight = 64;
            vec3_t origin = {0, 0, 24}, viewer = {1, 0, 0}, left = {0, -1, 0};
            polyVert_t vertices[4]{};
            doomsprite::BuildQuad(origin, viewer, left, patch, false, 24, vertices);
            Check(vertices[0].xyz[2] == 0, "Grounded sprite does not sink under collision floor");
            doomsprite::BuildQuad(origin, viewer, left, patch, false, 0, vertices, false);
            Check(vertices[0].xyz[2] == 19, "Airborne projectile retains original patch anchor");
        }
        Check(TypeForThing(2035) == Barrel && definitions[Barrel].health == 20,
              "Original barrel spawn maps to the replicated destructible prop");
        Check(Rotation(0, 22.49f) == 0 && Rotation(0, 22.51f) == 1 && Rotation(0, 359) == 0 &&
                  Rotation(0, -45) == 7 && Rotation(0, std::numeric_limits<float>::infinity()) == 0,
              "Angle boundaries and invalid values");
        auto imp = Read(base + "/models/players/imp/animation.cfg");
        SpriteSet set{};
        Check(Parse(imp.data(), set), "Imp config");
        Check(Sample(set, Missile, 0).frame == 4 && Sample(set, Missile, 457).frame == 5 &&
                  Sample(set, Missile, 458).frame == 6 && Sample(set, Missile, 458).event,
              "Imp releases its fireball after sixteen original Doom tics");
        Check(Sample(set, Death, 100000).frame == 12 && !set.clips[Death].loop,
              "Death holds final corpse frame");
        Check(Sample(set, Idle, 0).frame == Sample(set, Idle, 572).frame,
              "Idle loops after twenty Doom tics");
        Check(set.views[0][1].patch == set.views[0][7].patch &&
                  set.views[0][1].flip != set.views[0][7].flip,
              "Dual-name sprite rotations share a PNG and reverse their UVs");
        auto traversal = imp;
        traversal.replace(traversal.find("models/players/imp/frames/"), 25,
                          "models/players/../frames/");
        Check(!Parse(traversal.data(), set), "Path traversal must reject");
        std::puts(
            "Doom monster assets, animation, rotation, skill and malformed-input checks passed.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
