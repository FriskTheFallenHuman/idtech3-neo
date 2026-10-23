// SPDX-License-Identifier: GPL-3.0-or-later
// Original IWAD status-bar patches and positions, with tiled widescreen edges.
// The host's player state supplies the live health, armor, ammo, keys and weapons.
#include "cg_local.h"
#include "cg_doomhud.h"
#include "../game/bg_doomweapons.h"

namespace {
struct DoomPatch {
    const char *name;
    int width,height,left,top,textureWidth,textureHeight;
    qhandle_t shader;
};
#include "../qcommon/doom_ui_assets.h"
float sx,sy,originX;
int oldHealth=-1,faceUntil=0,nextLook=0,look=0,fireStart=0;
int oldWeapons[MAX_WEAPONS/(sizeof(int)*8)];
char expression[16];

int Clamp(int n,int lo,int hi) { return n<lo ? lo : n>hi ? hi : n; }
DoomPatch *Find(const char *name) {
    for (auto &p:doomPatches) if (!Q_stricmp(p.name,name)) return &p;
    return NULL;
}
void Register(DoomPatch *p) {
    if (!p->shader) {
        char path[MAX_QPATH]; Com_sprintf(path,sizeof(path),"ui/doom/%s",p->name);
        Q_strlwr(path); p->shader=trap_R_RegisterShaderNoMip(path);
    }
}
void Patch(const char *name,float x,float y) {
    DoomPatch *p=Find(name); if (!p) return; Register(p);
    trap_R_DrawStretchPic(originX+(x-p->left)*sx,(y-p->top)*sy,
        p->width*sx,p->height*sy,0,0,(float)p->width/p->textureWidth,(float)p->height/p->textureHeight,p->shader);
}
void Number(const char *prefix,int right,int y,int value,int digits=3) {
    bool negative=value<0; value=Clamp(abs(value),0,digits==3 ? 999:99);
    DoomPatch *zero=Find(va("%s0",prefix)); if (!zero) return;
    do {
        right-=zero->width;
        Patch(va("%s%d",prefix,value%10),right,y);
        value/=10;
    } while (value && --digits);
    if (negative) Patch("STTMINUS",right-8,y);
}
bool Has(const playerState_t &ps,int weapon) { return (ps.weapons[weapon/32]&(1u<<(weapon%32)))!=0; }
int Ammo(const playerState_t &ps,int weapon) {
    if (const auto *definition = doomweapon::Get(weapon)) {
        return definition->cost ? ps.ammo[definition->ammo] : 0;
    }
    int index=BG_FindAmmoForWeapon(static_cast<weapon_t>(weapon));
    int value=ps.ammo[index];
    // Include each owned weapon's loaded magazine, counting shared clips once.
    bool counted[MAX_WEAPONS]={};
    for (int w=1;w<WP_NUM_WEAPONS;++w) {
        if (!Has(ps,w) || BG_FindAmmoForWeapon(static_cast<weapon_t>(w))!=index) continue;
        int clip=BG_FindClipForWeapon(static_cast<weapon_t>(w));
        if (clip>=0 && clip<MAX_WEAPONS && !counted[clip]) { value+=ps.ammoclip[clip]; counted[clip]=true; }
    }
    return value;
}
const char *Face(const playerState_t &ps) {
    int health=ps.stats[STAT_HEALTH];
    int pain=Clamp((100-Clamp(health,0,100))*5/101,0,4);
    bool acquired=false;
    for (unsigned i=0;i<sizeof(oldWeapons)/sizeof(oldWeapons[0]);++i) {
        if (oldHealth>=0 && (ps.weapons[i]&~oldWeapons[i])) acquired=true;
        oldWeapons[i]=ps.weapons[i];
    }
    if (health<=0) { oldHealth=health; return "STFDEAD0"; }
    if (ps.powerups[PW_INVULNERABLE]) { oldHealth=health; return "STFGOD0"; }
    if (oldHealth>=0 && health<oldHealth) {
        if (oldHealth-health>20) Com_sprintf(expression,sizeof(expression),"STFOUCH%d",pain);
        else if (cg.damageX>.3f) Com_sprintf(expression,sizeof(expression),"STFTR%d0",pain);
        else if (cg.damageX<-.3f) Com_sprintf(expression,sizeof(expression),"STFTL%d0",pain);
        else Com_sprintf(expression,sizeof(expression),"STFKILL%d",pain);
        faceUntil=cg.time+1000;
    } else if (acquired && faceUntil<=cg.time) {
        Com_sprintf(expression,sizeof(expression),"STFEVL%d",pain); faceUntil=cg.time+2000;
    }
    oldHealth=health;
    if (faceUntil>cg.time) return expression;
    if ((ps.eFlags&EF_FIRING) || ps.weaponstate==WEAPON_FIRING || ps.weaponstate==WEAPON_FIRINGALT) {
        if (!fireStart) fireStart=cg.time;
        if (cg.time-fireStart>2000) return va("STFKILL%d",pain);
    } else fireStart=0;
    if (cg.time>=nextLook) { look=(cg.time/500*17+ps.clientNum)%3; nextLook=cg.time+500; }
    return va("STFST%d%d",pain,look);
}
void ExtendBar() {
    if (cgs.glconfig.vidWidth * 3 <= cgs.glconfig.vidHeight * 4 || originX <= 0) {
        return;
    }
    auto *background = Find("FLOOR7_2");
    if (!background) {
        return;
    }
    Register(background);
    // Keep the original 64x64 tile at the same pixel aspect as the status bar.
    // Shared screen-space UVs keep both margins aligned, including partial tiles.
    const float tileWidth = 64 * sx;
    const float tileHeight = 64 * sy;
    const float y = 168 * sy;
    trap_R_DrawStretchPic(0, y, originX, 32 * sy, 0, y / tileHeight, originX / tileWidth,
                          200 * sy / tileHeight, background->shader);
    const float right = originX + 320 * sx;
    trap_R_DrawStretchPic(right, y, cgs.glconfig.vidWidth - right, 32 * sy, right / tileWidth,
                          y / tileHeight, cgs.glconfig.vidWidth / tileWidth, 200 * sy / tileHeight,
                          background->shader);
}
} // namespace

void CG_DoomHudInit() {
    for (auto &p:doomPatches) p.shader=0;
    oldHealth=-1; faceUntil=nextLook=look=fireStart=0;
    memset(oldWeapons,0,sizeof(oldWeapons)); expression[0]=0;
}
qboolean CG_DoomHudVisible() {
    return cg_draw2D.integer && cg_drawStatus.integer && cg.snap && !cg.levelShot && !cg.cameraMode &&
        cg.snap->ps.pm_type!=PM_INTERMISSION && cg.snap->ps.persistant[PERS_TEAM]!=TEAM_SPECTATOR ? qtrue:qfalse;
}
int CG_DoomHudHeight() { return CG_DoomHudVisible() ? (int)(cgs.glconfig.vidHeight*.16f):0; }
void CG_DrawDoomHud() {
    if (!CG_DoomHudVisible()) return;
    const playerState_t &ps=cg.snap->ps;
    sy=cgs.glconfig.vidHeight/200.f; sx=sy/1.2f;
    originX=(cgs.glconfig.vidWidth-320*sx)*.5f;
    trap_R_SetColor(NULL);
    ExtendBar(); Patch("STBAR",0,168); Patch("STARMS",104,168);
    int weapon=ps.weapon;
    const auto *doomWeapon = doomweapon::Get(weapon);
    if (doomWeapon ? doomWeapon->cost > 0 : weapon > WP_KNIFE && weapon < WP_NUM_WEAPONS) {
        Number("STTNUM", 44, 171, Ammo(ps, weapon));
    }
    Number("STTNUM",90,171,ps.stats[STAT_HEALTH]); Patch("STTPRCNT",90,171);
    Number("STTNUM",221,171,ps.stats[STAT_ARMOR]); Patch("STTPRCNT",221,171);
    bool owned[] = {
        Has(ps, WP_LUGER) || Has(ps, WP_COLT) || Has(ps, WP_SILENCER) || Has(ps, WP_AKIMBO),
        Has(ps, WP_MAUSER) || Has(ps, WP_GARAND),
        Has(ps, WP_MP40) || Has(ps, WP_THOMPSON) || Has(ps, WP_STEN) || Has(ps, WP_VENOM),
        Has(ps, WP_PANZERFAUST),
        Has(ps, WP_FLAMETHROWER),
        Has(ps, WP_TESLA)};
    if (ps.stats[STAT_DOOM_WEAPONS]) {
        const int weapons[] = {WP_DOOM_PISTOL, WP_DOOM_SHOTGUN, WP_DOOM_CHAINGUN,
                               WP_DOOM_ROCKET, WP_DOOM_PLASMA,  WP_DOOM_BFG};
        for (int i = 0; i < 6; ++i) {
            owned[i] = Has(ps, weapons[i]);
        }
    }
    for (int i=0;i<6;++i) Patch(va(owned[i] ? "STYSNUM%d":"STGNUM%d",i+2),111+i%3*12,172+i/3*10);
    Patch(Face(ps),143,168);
    for (int i=0;i<3;++i) if (ps.stats[STAT_KEYS]&(1<<(i+1)))
        Patch(va("STKEYS%d",i+((ps.stats[STAT_KEYS]&(1<<(i+4))) ? 3:0)),239,171+i*10);
    int bulletWeapon=WP_LUGER;
    int currentAmmo=weapon>WP_NONE && weapon<WP_NUM_WEAPONS ? BG_FindAmmoForWeapon(static_cast<weapon_t>(weapon)) : WP_LUGER;
    if (currentAmmo==BG_FindAmmoForWeapon(WP_COLT)) bulletWeapon=WP_COLT;
    else if (currentAmmo==BG_FindAmmoForWeapon(WP_VENOM)) bulletWeapon=WP_VENOM;
    int types[] = {bulletWeapon, WP_MAUSER, WP_PANZERFAUST, WP_TESLA};
    if (ps.stats[STAT_DOOM_WEAPONS]) {
        const int doomAmmo[] = {WP_DOOM_PISTOL, WP_DOOM_SHOTGUN, WP_DOOM_ROCKET, WP_DOOM_PLASMA};
        memcpy(types, doomAmmo, sizeof(types));
    }
    const int y[]={173,179,185,191};
    for (int i=0;i<4;++i) {
        Number("STYSNUM",288,y[i],Ammo(ps,types[i]));
        Number("STYSNUM", 314, y[i],
               ps.stats[STAT_DOOM_WEAPONS] ? doomweapon::Maximum(ps, types[i])
                                           : ammoTable[types[i]].maxammo);
    }
    trap_R_SetColor(NULL);
}
