// SPDX-License-Identifier: GPL-3.0-or-later
#include "cg_local.h"
#include "cg_doomfx.h"
#include "cg_doomweapons.h"
#include "cg_doomitems.h"
#include "cg_doommap.h"
#include "../game/bg_doomweapons.h"
#include "../game/bg_doommonsters.h"

namespace {
constexpr int MaxPatches = 64, MaxFrames = 256;
struct Patch {
    char path[MAX_QPATH];
    int width, height, left, top, textureWidth, textureHeight;
    qhandle_t shader;
};
struct Frame {
    int body, flash, offsetY;
};
struct SpriteSet {
    Patch patches[MaxPatches];
    Frame frames[MaxFrames];
    int patchCount, frameCount;
    bool loaded;
};
SpriteSet sprites[8];
SpriteSet ammunition;
int lastWeapon = WP_NONE, lastAnimation = -1;
sfxHandle_t sawIdleSound;

int ReadNumber(char **text, int minimum, int maximum) {
    const char *token = COM_Parse(text);
    char *end = nullptr;
    const long value = strtol(token, &end, 10);
    if (!*token || !end || *end || value < minimum || value > maximum) {
        CG_Error("Invalid Doom sprite integer '%s' (%d..%d)", token, minimum, maximum);
    }
    return static_cast<int>(value);
}
void Expect(char **text, const char *expected) {
    if (Q_stricmp(COM_Parse(text), expected)) {
        CG_Error("Expected '%s' in Doom sprite configuration", expected);
    }
}
void Load(SpriteSet &set, const char *directory) {
    if (set.loaded) {
        return;
    }
    char path[MAX_QPATH], text[40000];
    Com_sprintf(path, sizeof(path), "models/weapons/%s/sprites.cfg", directory);
    fileHandle_t file;
    const int length = trap_FS_FOpenFile(path, &file, FS_READ);
    if (!file || length <= 0 || length >= static_cast<int>(sizeof(text))) {
        if (file) {
            trap_FS_FCloseFile(file);
        }
        CG_Error("Missing or oversized Doom sprite configuration: %s", path);
        return;
    }
    trap_FS_Read(text, length, file);
    trap_FS_FCloseFile(file);
    text[length] = 0;
    char *cursor = text;
    Expect(&cursor, "doomSprites");
    ReadNumber(&cursor, 1, 1);
    Expect(&cursor, "patches");
    set.patchCount = ReadNumber(&cursor, 1, MaxPatches);
    for (int i = 0; i < set.patchCount; ++i) {
        auto &patch = set.patches[i];
        const char *name = COM_Parse(&cursor);
        if (!*name || strlen(name) >= sizeof(patch.path) ||
            Q_stricmpn(name, "models/weapons/", 15) || strstr(name, "..")) {
            CG_Error("Invalid Doom sprite path in %s", path);
        }
        Q_strncpyz(patch.path, name, sizeof(patch.path));
        patch.width = ReadNumber(&cursor, 1, 1024);
        patch.height = ReadNumber(&cursor, 1, 1024);
        patch.left = ReadNumber(&cursor, -32768, 32767);
        patch.top = ReadNumber(&cursor, -32768, 32767);
        patch.textureWidth = ReadNumber(&cursor, patch.width, 1024);
        patch.textureHeight = ReadNumber(&cursor, patch.height, 1024);
        patch.shader = trap_R_RegisterShaderNoMip(patch.path);
        if (!patch.shader) {
            CG_Error("Missing Doom weapon sprite: %s", patch.path);
        }
    }
    Expect(&cursor, "frames");
    set.frameCount = ReadNumber(&cursor, 1, MaxFrames);
    for (int i = 0; i < set.frameCount; ++i) {
        auto &frame = set.frames[i];
        frame.body = ReadNumber(&cursor, 0, set.patchCount - 1);
        frame.flash = ReadNumber(&cursor, -1, set.patchCount - 1);
        frame.offsetY = ReadNumber(&cursor, 0, 96);
    }
    if (*COM_Parse(&cursor)) {
        CG_Error("Unexpected trailing Doom sprite configuration: %s", path);
    }
    set.loaded = true;
}
Patch *Find(int weapon, const char *name) {
    auto &set = sprites[weapon - WP_DOOM_FIST];
    const char *directory = doomweapon::Get(weapon)->directory;
    Load(set, directory);
    const char *path = va("models/weapons/%s/%s", directory, name);
    for (int i = 0; i < set.patchCount; ++i) {
        if (!Q_stricmp(set.patches[i].path, path)) {
            return &set.patches[i];
        }
    }
    return nullptr;
}
void Draw(const Patch &patch, int offsetY) {
    const float scaleY = cgs.glconfig.vidHeight / 200.0f;
    const float scaleX = scaleY / 1.2f;
    const float center = cg.refdef.x + cg.refdef.width * 0.5f;
    const float bottom = static_cast<float>(cg.refdef.y + cg.refdef.height);
    const float bobX = sin(cg.time * 0.007f) * min(cg.xyspeed * 0.012f, 3.0f);
    const float bobY = fabs(cos(cg.time * 0.007f)) * min(cg.xyspeed * 0.012f, 3.0f);
    float x = center + (-160 - patch.left + bobX) * scaleX;
    // Doom psprites use a 32-pixel base position in the full 200-pixel canvas.
    // The 3D viewport already excludes the status bar; anchoring to its bottom
    // raised the weapon by another 32 pixels.
    float y = cgs.glconfig.vidHeight + (-168 - patch.top + offsetY + bobY) * scaleY;
    float width = patch.width * scaleX, height = patch.height * scaleY;
    float s = 0, t = 0, s2 = static_cast<float>(patch.width) / patch.textureWidth;
    float t2 = static_cast<float>(patch.height) / patch.textureHeight;
    // Clip in screen space; lowered sprites never paint over the status bar.
    if (y + height > bottom) {
        t2 *= (bottom - y) / height;
        height = bottom - y;
    }
    if (y < cg.refdef.y) {
        const float clipped = cg.refdef.y - y;
        t = t2 * clipped / height;
        height -= clipped;
        y += clipped;
    }
    const float right = static_cast<float>(cg.refdef.x + cg.refdef.width);
    if (x + width > right) {
        s2 *= (right - x) / width;
        width = right - x;
    }
    if (x < cg.refdef.x) {
        const float clipped = cg.refdef.x - x;
        s = s2 * clipped / width;
        width -= clipped;
        x += clipped;
    }
    if (width > 0 && height > 0) {
        trap_R_DrawStretchPic(x, y, width, height, s, t, s2, t2, patch.shader);
    }
}
void Billboard(const Patch &patch, const vec3_t origin, bool centered, bool flip = false) {
    polyVert_t vertices[4]{};
    const float left = centered ? -patch.width * 0.5f : -static_cast<float>(patch.left);
    const float bottom = centered ? -patch.height * 0.5f : 0;
    const float horizontal[] = {left, left + patch.width, left + patch.width, left};
    const float vertical[] = {bottom, bottom, bottom + patch.height, bottom + patch.height};
    const float u = static_cast<float>(patch.width) / patch.textureWidth;
    const float v = static_cast<float>(patch.height) / patch.textureHeight;
    const float st[4][2] = {{0, v}, {u, v}, {u, 0}, {0, 0}};
    for (int i = 0; i < 4; ++i) {
        VectorMA(origin, -horizontal[i], cg.refdef.viewaxis[1], vertices[i].xyz);
        vertices[i].xyz[2] += vertical[i];
        vertices[i].st[0] = flip ? u - st[i][0] : st[i][0];
        vertices[i].st[1] = st[i][1];
        memset(vertices[i].modulate, 255, 4);
    }
    trap_R_AddPolyToScene(patch.shader, 4, vertices);
}
} // namespace

void CG_DoomWeaponsInit() {
    CG_DoomItemsInit();
    memset(sprites, 0, sizeof(sprites));
    memset(&ammunition, 0, sizeof(ammunition));
    lastWeapon = WP_NONE;
    lastAnimation = -1;
    sawIdleSound = 0;
}
void CG_RegisterDoomWeapon(int weapon) {
    if (!doomweapon::IsWeapon(weapon)) {
        return;
    }
    auto &info = cg_weapons[weapon];
    if (info.registered) {
        return;
    }
    auto &set = sprites[weapon - WP_DOOM_FIST];
    const auto *definition = doomweapon::Get(weapon);
    Load(set, definition->directory);
    const char *path = va("models/weapons/%s/weapon.cfg", definition->directory);
    if (!CG_ParseWeaponConfig(path, &info)) {
        CG_Error("Invalid Doom weapon animation: %s", path);
    }
    for (const auto &animation : info.weapAnimations) {
        if (animation.firstFrame < 0 || animation.numFrames < 1 || animation.frameLerp < 1 ||
            animation.firstFrame > set.frameCount - animation.numFrames) {
            CG_Error("Doom weapon animation outside sprite frames: %s", path);
        }
    }
    info.flashSound[0] = trap_S_RegisterSound(va("sound/doom/weapons/%s.wav", definition->sound));
    info.registered = qtrue;
}
void CG_DrawDoomWeapon() {
    auto &ps = cg.predictedPlayerState;
    if (!cg.snap || !ps.stats[STAT_DOOM_WEAPONS] || !doomweapon::IsWeapon(ps.weapon) ||
        cg.renderingThirdPerson || cg.cameraMode || cg.levelShot || ps.stats[STAT_HEALTH] <= 0 ||
        ps.pm_type == PM_INTERMISSION || ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
        return;
    }
    // The automap covers the view area, and Doom hides the weapon while it is up.
    if (CG_DoomMapVisible()) {
        return;
    }
    CG_RegisterDoomWeapon(ps.weapon);
    if (!doomweapon::Selectable(ps, cg.weaponSelect)) {
        cg.weaponSelect = ps.weapon;
    }
    auto &info = cg_weapons[ps.weapon];
    if (lastWeapon != ps.weapon) {
        memset(&cg.predictedPlayerEntity.pe.weap, 0, sizeof(lerpFrame_t));
        lastWeapon = ps.weapon;
        lastAnimation = -1;
        if (ps.weapon == WP_DOOM_CHAINSAW) {
            trap_S_StartLocalSound(trap_S_RegisterSound("sound/doom/weapons/sawup.wav"),
                                   CHAN_WEAPON);
        }
    }
    if (lastAnimation != ps.weapAnim && ps.weapon == WP_DOOM_BFG &&
        (ps.weapAnim & ~ANIM_TOGGLEBIT) == WEAP_ATTACK1) {
        trap_S_StartLocalSound(info.flashSound[0], CHAN_WEAPON);
    }
    lastAnimation = ps.weapAnim;
    if (ps.weapon == WP_DOOM_CHAINSAW && ps.weaponstate != WEAPON_FIRING) {
        if (!sawIdleSound) {
            sawIdleSound = trap_S_RegisterSound("sound/doom/weapons/sawidl.wav");
        }
        trap_S_AddLoopingSound(ps.clientNum, ps.origin, vec3_origin, sawIdleSound, 255);
    }
    if (!cg_drawGun.integer) {
        return;
    }
    int oldFrame, frame;
    float backlerp;
    CG_WeaponAnimation(&ps, &info, &oldFrame, &frame, &backlerp);
    const auto &set = sprites[ps.weapon - WP_DOOM_FIST];
    if (frame < 0 || frame >= set.frameCount) {
        CG_Error("Doom animation selected invalid frame %d", frame);
        return;
    }
    const auto &sprite = set.frames[frame];
    if (cg_debugAnim.integer & 4) {
        CG_Printf("Doom weapon=%d state=%d anim=%d time=%d delay=%d frame=%d offset=%d ammo=%d\n",
                  ps.weapon, ps.weaponstate, ps.weapAnim, ps.weaponTime, ps.weaponDelay, frame,
                  sprite.offsetY,
                  doomweapon::Get(ps.weapon)->cost ? ps.ammo[doomweapon::Get(ps.weapon)->ammo] : 0);
    }
    const bool invisible = ps.powerups[PW_INVIS] > cg.time &&
                           (ps.powerups[PW_INVIS] - cg.time > 4000 || ((cg.time / 115) & 1));
    // Doom dims the weapon sprite with the sector light at the player position
    vec4_t tint = {1.0f, 1.0f, 1.0f, 1.0f};
    vec3_t ambient, directed, lightDirection;
    if (trap_R_LightForPoint(cg.refdef.vieworg, ambient, directed, lightDirection)) {
        const float light = max(0.08f, min(ambient[0], 255.0f) / 255.0f);
        tint[0] = tint[1] = tint[2] = light;
    }
    if (invisible) {
        tint[0] *= 0.3f;
        tint[1] *= 0.3f;
        tint[2] *= 0.3f;
        tint[3] = 0.35f;
    }
    trap_R_SetColor(tint);
    // Presentation offsets use Doom's original 200-line canvas, independent of
    // resolution. The shotgun needs a smaller lift than the other weapons.
    constexpr int weaponLift[] = {12, 20, 6, 12, 12, 12, 12, 12};
    const int presentationOffset = sprite.offsetY - weaponLift[ps.weapon - WP_DOOM_FIST];
    Draw(set.patches[sprite.body], presentationOffset);
    if (sprite.flash >= 0) {
        Draw(set.patches[sprite.flash], presentationOffset);
    }
    trap_R_SetColor(nullptr);
}
void CG_DoomSelectWeapon(int slot, int direction) {
    if (!cg.snap || (cg.snap->ps.pm_flags & PMF_FOLLOW) ||
        cg.predictedPlayerState.stats[STAT_HEALTH] <= 0) {
        return;
    }
    const auto &ps = cg.predictedPlayerState;
    const int order[] = {WP_DOOM_FIST,     WP_DOOM_CHAINSAW, WP_DOOM_PISTOL, WP_DOOM_SHOTGUN,
                         WP_DOOM_CHAINGUN, WP_DOOM_ROCKET,   WP_DOOM_PLASMA, WP_DOOM_BFG};
    int selected = WP_NONE;
    if (direction) {
        int current = 0;
        for (int i = 0; i < 8; ++i) {
            if (order[i] == cg.weaponSelect) {
                current = i;
            }
        }
        for (int i = 1; i <= 8; ++i) {
            const int weapon = order[(current + direction * i + 8) % 8];
            if (doomweapon::Selectable(ps, weapon)) {
                selected = weapon;
                break;
            }
        }
    } else if (slot == 1) {
        selected = cg.weaponSelect == WP_DOOM_CHAINSAW       ? WP_DOOM_FIST
                   : doomweapon::Owned(ps, WP_DOOM_CHAINSAW) ? WP_DOOM_CHAINSAW
                                                             : WP_DOOM_FIST;
    } else if (slot >= 2 && slot <= 7) {
        const int slots[] = {WP_DOOM_PISTOL, WP_DOOM_SHOTGUN, WP_DOOM_CHAINGUN,
                             WP_DOOM_ROCKET, WP_DOOM_PLASMA,  WP_DOOM_BFG};
        selected = slots[slot - 2];
    }
    if (doomweapon::Selectable(ps, selected)) {
        cg.switchbackWeapon = cg.weaponSelect;
        cg.weaponSelect = selected;
        cg.weaponSelectTime = cg.time;
    }
}
void CG_DoomFireWeapon(centity_t *cent) {
    const int weapon = cent->currentState.weapon;
    CG_RegisterDoomWeapon(weapon);
    cent->muzzleFlashTime = cg.time;
    cg.lastFiredWeapon = weapon;
    if (weapon != WP_DOOM_BFG) {
        trap_S_StartSound(nullptr, cent->currentState.number, CHAN_WEAPON,
                          cg_weapons[weapon].flashSound[0]);
    }
}
void CG_DoomPickup(centity_t *cent) {
    CG_DoomItem(cent);
}
void CG_DoomMissile(centity_t *cent) {
    CG_DoomFxTrail(cent);
    const int weapon = cent->currentState.weapon;
    const int frame = cg.time / doomweapon::Milliseconds(4) % 2;
    const char *name = weapon == WP_DOOM_ROCKET   ? "misla1"
                       : weapon == WP_DOOM_PLASMA ? (frame ? "plssb0" : "plssa0")
                                                  : (frame ? "bfs1b0" : "bfs1a0");
    bool flip = false;
    if (weapon == WP_DOOM_ROCKET) {
        const char *rotations[] = {"misla1", "misla8a2", "misla7a3", "misla6a4",
                                   "misla5", "misla6a4", "misla7a3", "misla8a2"};
        vec3_t toViewer;
        VectorSubtract(cg.refdef.vieworg, cent->lerpOrigin, toViewer);
        const int rotation =
            doommonster::Rotation(vectoyaw(cent->currentState.pos.trDelta), vectoyaw(toViewer));
        name = rotations[rotation];
        flip = rotation >= 1 && rotation <= 3;
    }
    if (const Patch *patch = Find(weapon, name)) {
        Billboard(*patch, cent->lerpOrigin, true, flip);
    }
}
void CG_DoomImpact(int weapon, const vec3_t origin) {
    if (!doomweapon::IsWeapon(weapon) || weapon == WP_DOOM_FIST || weapon == WP_DOOM_CHAINSAW) {
        return;
    }
    auto *effect = CG_AllocLocalEntity();
    effect->leType = LE_DOOM_EXPLOSION;
    effect->startTime = cg.time;
    effect->endTime = cg.time + doomweapon::Milliseconds(weapon < WP_DOOM_ROCKET    ? 16
                                                         : weapon == WP_DOOM_ROCKET ? 18
                                                         : weapon == WP_DOOM_PLASMA ? 20
                                                                                    : 48);
    effect->refEntity.frame = weapon;
    VectorCopy(origin, effect->refEntity.origin);
    if (weapon < WP_DOOM_ROCKET) {
        return;
    }
    vec3_t soundOrigin;
    VectorCopy(origin, soundOrigin);
    trap_S_StartSound(soundOrigin, ENTITYNUM_WORLD, CHAN_AUTO,
                      trap_S_RegisterSound(weapon == WP_DOOM_ROCKET
                                               ? "sound/doom/weapons/rxplod.wav"
                                               : "sound/doom/weapons/firxpl.wav"));
}
void CG_DoomExplosion(localEntity_t *effect) {
    const int weapon = effect->refEntity.frame;
    const int elapsed = max(0, cg.time - effect->startTime);
    char name[16];
    if (weapon < WP_DOOM_ROCKET) {
        Com_sprintf(name, sizeof(name), "puff%c0",
                    'a' + min(3, elapsed / doomweapon::Milliseconds(4)));
    } else if (weapon == WP_DOOM_ROCKET) {
        Com_sprintf(name, sizeof(name), "misl%c0",
                    'b' + min(2, elapsed / doomweapon::Milliseconds(6)));
    } else if (weapon == WP_DOOM_PLASMA) {
        Com_sprintf(name, sizeof(name), "plse%c0",
                    'a' + min(4, elapsed / doomweapon::Milliseconds(4)));
    } else {
        Com_sprintf(name, sizeof(name), "bfe1%c0",
                    'a' + min(5, elapsed / doomweapon::Milliseconds(8)));
    }
    if (const Patch *patch = Find(weapon < WP_DOOM_ROCKET ? WP_DOOM_PISTOL : weapon, name)) {
        Billboard(*patch, effect->refEntity.origin, true);
    }
}

