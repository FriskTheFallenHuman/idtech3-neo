// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual first-person damage path with a fake renderer boundary.
#include "../cg_damageview.cpp"
#include <cstdio>
#include <stdexcept>
#include <vector>

cg_t cg{};
cgs_t cgs{};
namespace {
std::vector<refEntity_t> submitted;
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

void trap_R_AddRefEntityToScene(const refEntity_t *entity) {
    submitted.push_back(*entity);
}

int main() {
    try {
        snapshot_t snapshot{};
        cg.snap = &snapshot;
        cg.time = 1200;
        cg.viewDamage[0].damageValue = 30;
        cg.viewDamage[0].damageTime = 1000;
        cg.viewDamage[0].damageDuration = 1000;
        cg.viewDamage[0].damageX = 0.5f;
        cg.viewDamage[0].damageY = 0.3f;
        cg.v_dmg_time = 1500;
        cg.v_dmg_pitch = 5;
        cg.refdef.viewaxis[0][0] = cg.refdef.viewaxis[1][1] = cg.refdef.viewaxis[2][2] = 1;
        // A missing material is handle zero; a valid legacy material is retained below.
        snapshot.ps.stats[STAT_DOOM_WEAPONS] = 1;
        CG_DamageBlendBlob();
        Check(submitted.empty(), "Doom damage must not submit the missing blood sprite");
        Check(cg.v_dmg_time == 1500 && cg.v_dmg_pitch == 5,
              "Doom damage must retain screen flash and view kick timing");
        snapshot.ps.stats[STAT_DOOM_WEAPONS] = 0;
        CG_DamageBlendBlob();
        Check(submitted.empty(), "Missing legacy materials must never submit default squares");
        snapshot.ps.stats[STAT_DOOM_WEAPONS] = 1;
        for (auto &shader : cgs.media.viewBloodAni) {
            shader = 42;
        }
        CG_DamageBlendBlob();
        Check(submitted.empty(), "Doom must use screen flash even if legacy blood exists");
        snapshot.ps.stats[STAT_DOOM_WEAPONS] = 0;
        CG_DamageBlendBlob();
        Check(submitted.size() == 1 && submitted[0].reType == RT_SPRITE &&
                  submitted[0].customShader == 42 && submitted[0].radius > 0,
              "Legacy damage must retain the configured blood sprite");
        submitted.clear();
        snapshot.ps.stats[STAT_DOOM_WEAPONS] = 1;
        for (int offset = 0; offset < 1200; offset += 25) {
            cg.time = 1000 + offset;
            CG_DamageBlendBlob();
        }
        Check(submitted.empty(), "No animation frame may submit a Doom damage blob");
        std::puts(
            "Doom damage view: no blood sprites, preserved flash/kick and legacy effects passed.");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
