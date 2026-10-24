// SPDX-License-Identifier: GPL-3.0-or-later
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include "game/cgame/cg_doomhud.cpp"

cg_t cg;
cgs_t cgs;
vmCvar_t cg_draw2D,cg_drawStatus;
ammotable_t ammoTable[WP_NUM_WEAPONS];
namespace test {
std::map<int, std::string> shaders;
std::vector<std::string> draws;
struct Rectangle {
    std::string shader;
    float x, y, width, height, s1, t1, s2, t2;
};
std::vector<Rectangle> rectangles;
int next=1;
void Check(bool ok,const char *why) { if (!ok) throw std::runtime_error(why); }
bool Drawn(const char *s) { for (auto &d:draws) if (d==s) return true; return false; }
void Draw() {
    draws.clear();
    rectangles.clear();
    CG_DrawDoomHud();
}
}
int Q_stricmp(const char *a,const char *b) { return _stricmp(a,b); }
char *Q_strlwr(char *s) { for (char *p=s;*p;++p) *p=(char)tolower(*p); return s; }
void QDECL Com_sprintf(char *d,int n,const char *fmt,...) {
    va_list args; va_start(args,fmt); vsnprintf(d,n,fmt,args); va_end(args);
}
char *QDECL va(const char *fmt,...) {
    static char buffers[8][4096]; static int index; char *result=buffers[index++%8];
    va_list args; va_start(args,fmt); vsnprintf(result,4096,fmt,args); va_end(args); return result;
}
qhandle_t trap_R_RegisterShaderNoMip(const char *s) { int h=test::next++; test::shaders[h]=s; return h; }
void trap_R_SetColor(const float *) {}
void trap_R_DrawStretchPic(float x, float y, float width, float height, float s1, float t1,
                           float s2, float t2, qhandle_t shader) {
    test::draws.push_back(test::shaders[shader]);
    test::rectangles.push_back({test::shaders[shader], x, y, width, height, s1, t1, s2, t2});
}
weapon_t BG_FindAmmoForWeapon(weapon_t weapon) {
    if (weapon == WP_MP40 || weapon == WP_SILENCER)
        return WP_LUGER;
    return weapon;
}
weapon_t BG_FindClipForWeapon(weapon_t weapon) { return weapon==WP_SILENCER ? WP_LUGER:weapon; }

int main() {
    using namespace test;
    try {
        snapshot_t snap={}; cg.snap=&snap;
        cg_draw2D.integer=cg_drawStatus.integer=1;
        cgs.glconfig.vidWidth=1280; cgs.glconfig.vidHeight=720;
        auto &ps=snap.ps; ps.stats[STAT_HEALTH]=100; ps.weapon=WP_LUGER;
        ps.weapons[0]=(1<<WP_LUGER)|(1<<WP_SILENCER)|(1<<WP_MP40);
        ps.ammo[WP_LUGER]=40; ps.ammoclip[WP_LUGER]=8; ps.ammoclip[WP_MP40]=32;
        CG_DoomHudInit(); cg.time=1000; Draw();
        Check(Drawn("ui/doom/stbar") && Drawn("ui/doom/starms"),"HUD must draw original bar and weapon panel");
        Check(CG_DoomHudVisible() && CG_DoomHudHeight()==115,"HUD must reserve its status-bar area");
        Check(Ammo(ps,WP_LUGER)==80,"Ammo must include magazines without double-counting shared clips");
        Check(fabs(originX-160)<.001 && fabs(sx-3)<.001 && fabs(sy-3.6f)<.001,"HUD must center and preserve Doom pixel aspect on widescreen");
        const int resolutions[][2] = {
            {1280, 720}, {1920, 1080}, {2560, 1080}, {1365, 768}, {1024, 768}};
        for (const auto &resolution : resolutions) {
            cgs.glconfig.vidWidth = resolution[0];
            cgs.glconfig.vidHeight = resolution[1];
            Draw();
            int backgrounds = 0, bars = 0;
            float marginWidth = 0;
            for (const auto &rectangle : rectangles) {
                if (rectangle.shader == "ui/doom/stbar") {
                    ++bars;
                }
                if (rectangle.shader != "ui/doom/floor7_2") {
                    continue;
                }
                ++backgrounds;
                marginWidth += rectangle.width;
                Check(rectangle.width > 0 && fabs(rectangle.y - resolution[1] * 0.84f) < 0.01f &&
                          fabs(rectangle.height - resolution[1] * 0.16f) < 0.01f,
                      "HUD background fills only the bottom margins");
                Check(fabs((rectangle.s2 - rectangle.s1) * 64 * sx - rectangle.width) < 0.01f &&
                          fabs((rectangle.t2 - rectangle.t1) * 64 * sy - rectangle.height) < 0.01f,
                      "HUD background preserves original tile scale without stretching");
                Check(rectangle.x + rectangle.width <= originX + 0.01f ||
                          rectangle.x >= originX + 320 * sx - 0.01f,
                      "Background margins do not overwrite the centered status bar");
            }
            Check(bars == 1 && backgrounds == (resolution[0] * 3 > resolution[1] * 4 ? 2 : 0),
                  "Draw one original status bar and exactly two widescreen background margins");
            Check(fabs(marginWidth + 320 * sx - resolution[0]) < 0.01f,
                  "HUD margins cover the full width without gaps at odd resolutions");
        }
        cgs.glconfig.vidWidth = 1280;
        cgs.glconfig.vidHeight = 720;
        ps.stats[STAT_HEALTH] = 60;
        cg.time = 1100;
        Draw();
        Check(Drawn("ui/doom/stfouch1"),
              "Large health loss must use the matching pain-tier ouch face");
        ps.stats[STAT_KEYS] = (1 << 1) | (1 << 5);
        Draw();
        Check(Drawn("ui/doom/stkeys0") && !Drawn("ui/doom/stkeys4"),
              "Skull marker alone must not grant a color key");
        ps.stats[STAT_KEYS] |= (1 << 2);
        Draw();
        Check(Drawn("ui/doom/stkeys4"),"Collected skull must use the original skull-key patch");
        cg.time=2500; ps.weapons[0]|=(1<<WP_PANZERFAUST); Draw();
        Check(Drawn("ui/doom/stfevl1"),"New weapon must trigger grin in the current pain tier");
        ps.powerups[PW_INVULNERABLE]=9999; Draw();
        Check(Drawn("ui/doom/stfgod0"),"Invulnerability must select god face");
        ps.stats[STAT_HEALTH]=0; Draw();
        Check(Drawn("ui/doom/stfdead0"),"Death face must remain visible even during invulnerability");
        cg_drawStatus.integer=0; Draw(); Check(draws.empty() && CG_DoomHudHeight()==0,"Hidden HUD must release its viewport area");
        cg_drawStatus.integer=1; cg.levelShot=qtrue; Check(!CG_DoomHudVisible(),"Levelshots must hide HUD");
        cg.levelShot=qfalse; cg.cameraMode=qtrue; Check(!CG_DoomHudVisible(),"Cinematics must hide HUD");
        cg.cameraMode=qfalse; ps.pm_type=PM_INTERMISSION; Check(!CG_DoomHudVisible(),"Intermissions must retain their own layout");
        ps.pm_type = PM_NORMAL;
        ps.stats[STAT_HEALTH] = 100;
        ps.powerups[PW_INVULNERABLE] = 0;
        doomweapon::Initialize(ps);
        ps.weapon = WP_DOOM_FIST;
        ps.ammo[WP_DOOM_PISTOL] = 50;
        Check(Ammo(ps, WP_DOOM_PISTOL) == 50 && Ammo(ps, WP_DOOM_CHAINGUN) == 50,
              "Doom HUD shares pistol/chaingun bullets without magazine accounting");
        ps.ammo[WP_DOOM_PLASMA] = 123;
        Check(Ammo(ps, WP_DOOM_PLASMA) == 123 && Ammo(ps, WP_DOOM_BFG) == 123,
              "Doom HUD shares plasma/BFG cells");
        Draw();
        Check(!Drawn("ui/doom/stysnum6"), "Unowned plasma slot is gray");
        ps.weapons[WP_DOOM_PLASMA / 32] |= 1u << (WP_DOOM_PLASMA % 32);
        Draw();
        Check(Drawn("ui/doom/stysnum6"), "Doom weapon ownership lights the matching slot");
        puts("Doom HUD checks passed: patches, widescreen, Doom shared ammo/ownership, legacy "
             "ammo, faces, keys and hidden viewport.");
        return 0;
    } catch (const std::exception &e) { fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
