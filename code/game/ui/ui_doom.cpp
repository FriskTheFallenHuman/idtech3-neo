// SPDX-License-Identifier: GPL-3.0-or-later
// Doom's menus, using the IWAD's unmodified patches.
// Layout and interaction reference: id Software's linuxdoom-1.10/m_menu.c.
#include "ui_local.h"
#include "ui_doom.h"
#include "../game/bg_doomlevel.h"

namespace {
struct DoomPatch {
    const char *name;
    int width, height, left, top, textureWidth, textureHeight;
    qhandle_t shader;
};
#include "../qcommon/doom_ui_assets.h"

enum Screen { Main, Episode, Skill, Options, Sound, Load, Save, Help, Credits, Multiplayer, Join, ScreenCount };
enum Prompt { NoPrompt, Notice, Quit, EndGame, Nightmare };
struct Menu {
    int x, y, count;
    const char *patches[8];
    const char *hotkeys;
};
const Menu menus[] = {
    {97,64,7,{"M_NGAME","M_MULTI","M_OPTION","M_LOADG","M_SAVEG","M_RDTHIS","M_QUITG"},"nmolsrq"},
    {48,63,3,{"M_EPI1","M_EPI2","M_EPI3"},"kti"},
    {48,63,5,{"M_JKILL","M_ROUGH","M_HURT","M_ULTRA","M_NMARE"},"ihhun"},
    {60,37,8,{"M_ENDGAM","M_MESSG","M_DETAIL","M_SCRNSZ","","M_MSENS","","M_SVOL"},"emgs m s"},
    {80,64,4,{"M_SFXVOL","","M_MUSVOL",""},"s m "},
    {80,54,6,{"","","","","",""},"123456"},
    {80,54,6,{"","","","","",""},"123456"},
    {280,185,1,{""},""},
    {330,175,1,{""},""},
    {44,50,8,{"","","","","","","",""},"dmbsfthj"},
    {44,70,2,{"",""},"aj"}
};
qboolean active = qfalse, inGame = qfalse, editing = qfalse;
qboolean titleOnly = qfalse;
Screen screen = Main;
Prompt prompt = NoPrompt;
int selected[ScreenCount] = {0,0,2}, episode = 1, openedAt = 0;
char message[512], descriptions[6][24], oldDescription[24];
qboolean occupied[6];
float mouseX = 160, mouseY = 100, mouseTravel = 0;
qhandle_t white;
char serverAddress[64] = "localhost:27960";
qboolean editingAddress = qfalse;
float sx, sy, ox, oy;

int Clamp(int value, int lo, int hi) { return value < lo ? lo : value > hi ? hi : value; }
DoomPatch *FindPatch(const char *name) {
    for (auto &patch : doomPatches) if (!Q_stricmp(patch.name, name)) return &patch;
    return NULL;
}
void Geometry() {
    const auto &gl = uiInfo.uiDC.glconfig;
    // Doom's 320x200 pixels have a 5:6 pixel aspect ratio on a 4:3 monitor.
    float height = (float)gl.vidHeight;
    if (gl.vidWidth * 3 < gl.vidHeight * 4) height = gl.vidWidth * .75f;
    sx = height / 240.f; sy = height / 200.f;
    ox = (gl.vidWidth - 320 * sx) * .5f;
    oy = (gl.vidHeight - 200 * sy) * .5f;
}
void Patch(const char *name, float x, float y) {
    DoomPatch *p = FindPatch(name);
    if (!p) return;
    if (!p->shader) {
        char path[MAX_QPATH];
        Com_sprintf(path, sizeof(path), "ui/doom/%s", p->name);
        Q_strlwr(path);
        p->shader = trap_R_RegisterShaderNoMip(path);
    }
    trap_R_DrawStretchPic(ox + (x-p->left)*sx, oy + (y-p->top)*sy,
        p->width*sx, p->height*sy, 0, 0,
        (float)p->width/p->textureWidth, (float)p->height/p->textureHeight, p->shader);
}
int CharWidth(int c) {
    if (c < 33 || c > 95) return 4;
    DoomPatch *p = FindPatch(va("STCFN%03d", c));
    return p ? p->width : 4;
}
int TextWidth(const char *s) {
    int width = 0;
    while (*s && *s != '\n') width += CharWidth(toupper((unsigned char)*s++));
    return width;
}
void Text(int x, int y, const char *s) {
    int start = x;
    for (; *s; ++s) {
        int c = toupper((unsigned char)*s);
        if (c == '\n') { x = start; y += 12; continue; }
        if (c >= 33 && c <= 95) Patch(va("STCFN%03d",c),x,y);
        x += CharWidth(c);
    }
}
void CenterMessage() {
    int lines = 1;
    for (const char *p = message; *p; ++p) if (*p == '\n') ++lines;
    int y = 100 - lines * 7 / 2;
    const char *p = message;
    do {
        char line[128]; int n = 0;
        while (*p && *p != '\n') { if (n < 127) line[n++] = *p; ++p; }
        line[n] = 0;
        Text(160-TextWidth(line)/2,y,line);
        y += 7;
        if (!*p) break;
        ++p;
    } while (true);
}
void Play(const char *sound) {
    trap_S_StartLocalSound(trap_S_RegisterSound(va("sound/doom/menu/%s.wav",sound)),CHAN_LOCAL_SOUND);
}
void Message(Prompt kind, const char *text) {
    prompt = kind; Q_strncpyz(message,text,sizeof(message));
}
void Close() {
    active = editing = qfalse; prompt = NoPrompt;
    _UI_SetActiveMenu(UIMENU_NONE);
}
void ReadSlots() {
    for (int i=0;i<6;++i) {
        fileHandle_t f;
        int length = trap_FS_FOpenFile(va("save/doomslot%d.svg",i),&f,FS_READ);
        occupied[i] = length > 0 ? qtrue : qfalse;
        if (f) trap_FS_FCloseFile(f);
        Q_strncpyz(descriptions[i],occupied[i] ? "SAVED GAME" : "EMPTY SLOT",24);
        length = trap_FS_FOpenFile(va("save/doomslot%d.txt",i),&f,FS_READ);
        if (f) {
            if (occupied[i] && length > 0) {
                int n = Clamp(length,0,23);
                trap_FS_Read(descriptions[i],n,f); descriptions[i][n]=0;
                for (int j=0;j<n;++j) if ((unsigned char)descriptions[i][j]<32) descriptions[i][j]=' ';
            }
            trap_FS_FCloseFile(f);
        }
    }
}
void Go(Screen next) { screen=next; editing=editingAddress=qfalse; mouseTravel=0; }
bool DemoEpisode(int chosenEpisode) {
    if (chosenEpisode <= 1 || !trap_Cvar_VariableValue("doom_demo")) {
        return false;
    }
    // Like M_Episode in the original Doom, retain the episode rows and show help
    // after explaining why the selected registered episode is unavailable.
    Go(Help);
    // SWSTRING from linuxdoom-1.10/d_englsh.h (id Software, 1993-1996).
    Message(Notice, "THIS IS THE SHAREWARE VERSION OF DOOM.\n\n"
                    "YOU NEED TO ORDER THE ENTIRE TRILOGY.\n\nPRESS A KEY.");
    return true;
}
void StartGame() {
    if (DemoEpisode(episode)) {
        return;
    }
    // Preserve all five Doom choices while mapping onto the host's four skill levels.
    int choice=selected[Skill];
    trap_Cvar_Set("doom_skill",va("%d",choice));
    trap_Cvar_Set("g_spSkill",va("%d",choice+1));
    trap_Cvar_Set("g_gameskill",va("%d",choice < 2 ? 0 : choice-1));
    int chosenEpisode=episode;
    Close();
    trap_Cvar_Set("g_reloading","0");
    trap_Cvar_Set("doom_carry", "");
    trap_Cvar_Set("doom_visited", "0");
    trap_Cvar_Set("doom_visited_episode", "0");
    trap_Cmd_ExecuteText(EXEC_APPEND,va("spmap e%dm1\n",chosenEpisode));
}
void SaveSlot() {
    int slot=selected[Save];
    if (!descriptions[slot][0]) return;
    fileHandle_t f;
    trap_FS_FOpenFile(va("save/doomslot%d.txt",slot),&f,FS_WRITE);
    if (f) { trap_FS_Write(descriptions[slot],strlen(descriptions[slot]),f); trap_FS_FCloseFile(f); }
    Close();
    trap_Cmd_ExecuteText(EXEC_APPEND,va("savegame doomslot%d\n",slot));
}
int CvarInt(const char *name) { return (int)trap_Cvar_VariableValue(name); }
bool AddressCharacter(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '.' || c == ':' || c == '-' || c == '_' ||
        c == '[' || c == ']';
}
void JoinServer() {
    if (!serverAddress[0]) {
        Message(Notice, "ENTER A SERVER ADDRESS.");
        return;
    }
    for (const char *p = serverAddress; *p; ++p) {
        if (!AddressCharacter(static_cast<unsigned char>(*p))) {
            Message(Notice, "INVALID SERVER ADDRESS.");
            return;
        }
    }
    trap_Cvar_Set("ui_doomServer", serverAddress);
    Close();
    trap_Cmd_ExecuteText(EXEC_APPEND, va("connect %s\n", serverAddress));
}
void HostGame() {
    const int map = Clamp(CvarInt("ui_doomMpMap"), 0, 26);
    if (DemoEpisode(map / 9 + 1)) {
        return;
    }
    trap_Cvar_Set("g_gametype", CvarInt("ui_doomMpTeam") ? "3" : "0");
    trap_Cvar_Set("sv_maxclients", "16");
    trap_Cvar_Set("g_maxGameClients", "16");
    trap_Cvar_Set("bot_enable", "1");
    trap_Cvar_Set("g_doWarmup", "0");
    trap_Cvar_Set("g_reloading", "0");
    trap_Cvar_Set("doom_carry", "");
    trap_Cvar_Set("doom_botCount", va("%d", Clamp(CvarInt("ui_doomMpBots"), 0, 15)));
    trap_Cvar_Set("doom_botSkill", va("%d", Clamp(CvarInt("ui_doomMpSkill"), 1, 5)));
    for (int bot = 0; bot < 16; ++bot) {
        trap_Cvar_Set(va("doom_botRoster%d", bot), "");
    }
    trap_Cvar_Set("fraglimit", va("%d", Clamp(CvarInt("ui_doomMpFrags"), 0, 100)));
    trap_Cvar_Set("timelimit", va("%d", Clamp(CvarInt("ui_doomMpMinutes"), 0, 60)));
    trap_Cvar_Set("g_friendlyFire", "0");
    Close();
    trap_Cmd_ExecuteText(EXEC_APPEND, va("map e%dm%d\n", map / 9 + 1, map % 9 + 1));
}
void LargeText(int x, int y, const char *text) {
    const float oldSx = sx, oldSy = sy;
    sx *= 2; sy *= 2;
    int at = x;
    for (const char *p = text; *p; ++p) {
        const int c = toupper(static_cast<unsigned char>(*p));
        if (c >= 33 && c <= 95) Patch(va("STCFN%03d", c), at * .5f, y * .5f);
        at += CharWidth(c) * 2;
    }
    sx = oldSx; sy = oldSy;
}
void Adjust(int direction) {
    int item=selected[screen];
    if (screen == Multiplayer) {
        const char *names[] = {"ui_doomMpTeam", "ui_doomMpMap", "ui_doomMpBots",
            "ui_doomMpSkill", "ui_doomMpFrags", "ui_doomMpMinutes"};
        const int minimum[] = {0, 0, 0, 1, 0, 0};
        const int maximum[] = {1, CvarInt("doom_demo") ? 8 : 26, 15, 5, 100, 60};
        if (item >= 6) return;
        const int value = Clamp(CvarInt(names[item]) + direction, minimum[item], maximum[item]);
        trap_Cvar_Set(names[item], va("%d", value));
    } else if (screen==Sound) {
        const char *name=item==0 ? "s_volume" : "s_musicvolume";
        int value=Clamp((int)(trap_Cvar_VariableValue(name)*15+.5f)+direction,0,15);
        trap_Cvar_Set(name,va("%f",value/15.f));
    } else if (screen==Options) {
        switch(item) {
        case 1: trap_Cvar_Set("doom_messages",CvarInt("doom_messages") ? "0":"1");
                trap_Cvar_Set("con_notifytime",CvarInt("doom_messages") ? "3":"0"); break;
        case 2: trap_Cvar_Set("doom_detail",CvarInt("doom_detail") ? "0":"1");
                trap_Cvar_Set("r_lodbias",CvarInt("doom_detail") ? "1":"0"); break;
        case 3: trap_Cvar_Set("cg_viewsize",va("%d",Clamp(CvarInt("cg_viewsize")+direction*10,30,100))); break;
        case 5: trap_Cvar_Set("sensitivity",va("%d",Clamp(CvarInt("sensitivity")+direction,0,9))); break;
        default: return;
        }
    } else return;
    Play("stnmov");
}
void Activate() {
    int item=selected[screen];
    Play("pistol");
    switch(screen) {
    case Main:
        switch(item) {
        case 0: Go(Episode); break;
        case 1: Go(Multiplayer); break;
        case 2: Go(Options); break;
        case 3: ReadSlots(); Go(Load); break;
        case 4:
            if (inGame && CvarInt("g_gametype") != GT_SINGLE_PLAYER) { Message(Notice, "SAVING IS FOR SINGLE PLAYER."); break; }
            if (!inGame) Message(Notice,"YOU CAN'T SAVE IF YOU AREN'T\nPLAYING!\n\nPRESS A KEY.");
            else { ReadSlots(); Go(Save); }
            break;
        case 5: Go(Help); break;
        case 6: Message(Quit,"ARE YOU SURE YOU WANT TO\nQUIT THIS GREAT GAME?\n\n(PRESS Y TO QUIT)"); break;
        } break;
    case Multiplayer:
        if (item == 6) HostGame();
        else if (item == 7) Go(Join);
        else Adjust(1);
        break;
    case Join:
        if (item == 0) editingAddress = qtrue;
        else JoinServer();
        break;
    case Episode:
        if (!DemoEpisode(item + 1)) {
            episode = item + 1;
            Go(Skill);
        }
        break;
    case Skill:
        if (item == 4)
            Message(Nightmare,
                    "ARE YOU SURE? THIS SKILL LEVEL\nISN'T EVEN REMOTELY FAIR.\n\nPRESS Y OR N.");
        else StartGame();
        break;
    case Options:
        if (item==0) {
            if (inGame) Message(EndGame,"ARE YOU SURE YOU WANT TO\nEND THE GAME?\n\nPRESS Y OR N.");
            else Play("oof");
        } else if (item==7) Go(Sound);
        else Adjust(1);
        break;
    case Sound: Adjust(1); break;
    case Load:
        if (occupied[item]) { Close(); trap_Cmd_ExecuteText(EXEC_APPEND,va("loadgame doomslot%d\n",item)); }
        else Play("oof");
        break;
    case Save:
        editing=qtrue;
        Q_strncpyz(oldDescription,descriptions[item],24);
        if (!occupied[item]) descriptions[item][0]=0;
        break;
    case Help: Go(Credits); break;
    case Credits: Go(Main); break;
    default: break;
    }
}
qboolean Selectable(int row) {
    return !((screen==Options && (row==4 || row==6)) || (screen==Sound && (row==1 || row==3)));
}
void Move(int direction) {
    int &item=selected[screen]; const int count=menus[screen].count;
    do { item=(item+direction+count)%count; } while (!Selectable(item));
    mouseTravel=0; Play("pstop");
}
void Thermo(int x,int y,int width,int value) {
    Patch("M_THERML",x,y);
    for (int i=0;i<width;++i) Patch("M_THERMM",x+8+i*8,y);
    Patch("M_THERMR",x+8+width*8,y);
    Patch("M_THERMO",x+8+Clamp(value,0,width-1)*8,y);
}
} // namespace

void UI_DoomInit() {
    active=inGame=editing=qfalse; prompt=NoPrompt;
    for (auto &p:doomPatches) p.shader=0;
    white=trap_R_RegisterShaderNoMip("white");
    trap_Cvar_Register(NULL,"doom_messages","1",CVAR_ARCHIVE);
    trap_Cvar_Register(NULL,"doom_detail","0",CVAR_ARCHIVE);
    trap_Cvar_Register(NULL,"doom_skill","2",CVAR_ARCHIVE);
    trap_Cvar_Register(nullptr, "ui_doomMpTeam", "0", CVAR_ARCHIVE);
    trap_Cvar_Register(nullptr, "ui_doomMpMap", "0", CVAR_ARCHIVE);
    trap_Cvar_Register(nullptr, "ui_doomMpBots", "3", CVAR_ARCHIVE);
    trap_Cvar_Register(nullptr, "ui_doomMpSkill", "3", CVAR_ARCHIVE);
    trap_Cvar_Register(nullptr, "ui_doomMpFrags", "20", CVAR_ARCHIVE);
    trap_Cvar_Register(nullptr, "ui_doomMpMinutes", "10", CVAR_ARCHIVE);
    trap_Cvar_Register(nullptr, "ui_doomServer", "localhost:27960", CVAR_ARCHIVE);
    if (CvarInt("doom_demo")) {
        trap_Cvar_Set("ui_doomMpMap", va("%d", Clamp(CvarInt("ui_doomMpMap"), 0, 8)));
    }
    trap_Cvar_VariableStringBuffer("ui_doomServer", serverAddress, sizeof(serverAddress));
    // Existing saved configs predate these shortcuts; retain custom bindings.
    const int keys[] = {K_F1, K_F2, K_F3, K_F4, K_F7, K_F10, '5', '6', '7', K_F8};
    const char *commands[] = {"doommenu help",  "doommenu save",    "doommenu load",
                              "doommenu sound", "doommenu endgame", "doommenu quit",
                              "weaponbank 5",   "weaponbank 6",     "weaponbank 7", "+scores"};
    for (unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i) {
        char binding[256]; trap_Key_GetBindingBuf(keys[i],binding,sizeof(binding));
        if (!binding[0]) trap_Key_SetBinding(keys[i],commands[i]);
    }
    Geometry();
}
qboolean UI_DoomOpen(uiMenuCommand_t menu) {
    if (menu==UIMENU_NONE) { active=editing=qfalse; prompt=NoPrompt; return qfalse; }
    if (menu!=UIMENU_MAIN && menu!=UIMENU_INGAME) { active=qfalse; return qfalse; }
    // Leave engine error dialogs on their existing path.
    char error[256]; trap_Cvar_VariableStringBuffer("com_errorMessage",error,sizeof(error));
    if (menu==UIMENU_MAIN && error[0]) {
        inGame=qfalse; active=qtrue; titleOnly=qfalse; Go(Main);
        Message(Notice,error); trap_Cvar_Set("com_errorMessage", ""); return qtrue;
    }
    inGame=menu==UIMENU_INGAME ? qtrue:qfalse;
    active=qtrue; titleOnly=qfalse; prompt=NoPrompt; Go(Main);
    if (!inGame) {
        trap_S_StartBackgroundTrack("music/doom/d_intro.mid", "music/doom/d_intro.mid", 0);
    }
    openedAt=uiInfo.uiDC.realTime;
    Menus_CloseAll();
    trap_Key_SetCatcher(KEYCATCH_UI);
    trap_Cvar_Set("cl_paused", inGame && CvarInt("g_gametype") == GT_SINGLE_PLAYER ? "1" : "0");
    if (!inGame) { trap_Cvar_Set("g_reloading","0"); trap_S_FadeAllSound(1.f,0); }
    Play("swtchn");
    return qtrue;
}
qboolean UI_DoomActive() { return active; }
qboolean UI_DoomFullscreen() { return active && !inGame ? qtrue:qfalse; }
void UI_DoomDraw() {
    Geometry(); trap_R_SetColor(NULL);
    if (!inGame) {
        const vec4_t black={0,0,0,1}; trap_R_SetColor(black);
        trap_R_DrawStretchPic(0,0,(float)uiInfo.uiDC.glconfig.vidWidth,(float)uiInfo.uiDC.glconfig.vidHeight,0,0,1,1,white);
        trap_R_SetColor(NULL); Patch("TITLEPIC",0,0);
    }
    if (titleOnly) return;
    if (prompt!=NoPrompt) { CenterMessage(); return; }
    if (screen==Help || screen==Credits) {
        Patch(screen==Help ? "HELP1":"CREDIT",0,0);
        if (screen==Help) Patch(((uiInfo.uiDC.realTime-openedAt)*35/8000)&1 ? "M_SKULL2":"M_SKULL1",248,180);
        return;
    }
    const Menu &menu=menus[screen];
    switch(screen) {
    case Main: Patch("M_DOOM",94,2); break;
    case Multiplayer: {
        Patch("M_MULTI", 44, 18);
        const int map = Clamp(CvarInt("ui_doomMpMap"), 0, 26);
        Text(44, 50, CvarInt("ui_doomMpTeam") ? "MODE: TEAM DEATHMATCH" : "MODE: DEATHMATCH");
        Text(44, 66, va("MAP: E%dM%d", map / 9 + 1, map % 9 + 1));
        Text(44, 82, va("BOTS: %d", Clamp(CvarInt("ui_doomMpBots"), 0, 15)));
        Text(44, 98, va("BOT SKILL: %d", Clamp(CvarInt("ui_doomMpSkill"), 1, 5)));
        Text(44, 114, va("FRAG LIMIT: %d", Clamp(CvarInt("ui_doomMpFrags"), 0, 100)));
        Text(44, 130, va("MINUTES: %d", Clamp(CvarInt("ui_doomMpMinutes"), 0, 60)));
        Text(44, 146, "HOST GAME"); Text(44, 162, "JOIN SERVER");
        Text(44, 186, "LEFT/RIGHT TO CHANGE");
        break;
    }
    case Join: {
        LargeText(44, 28, "JOIN SERVER");
        Text(44, 50, "ADDRESS:");
        const int length = static_cast<int>(strlen(serverAddress));
        Text(44, 62, va("%s%s", serverAddress + (length > 28 ? length - 28 : 0), editingAddress ? "_" : ""));
        Text(44, 86, "JOIN GAME");
        Text(44, 146, "ENTER ADDRESS, THEN JOIN GAME");
        break;
    }
    case Episode: Patch("M_EPISOD",54,38); break;
    case Skill: Patch("M_NEWG",96,14); Patch("M_SKILL",54,38); break;
    case Options:
        Patch("M_OPTTTL",108,15);
        Patch(CvarInt("doom_messages") ? "M_MSGON":"M_MSGOFF",180,53);
        Patch(CvarInt("doom_detail") ? "M_GDLOW":"M_GDHIGH",235,69);
        Thermo(60,101,9,(CvarInt("cg_viewsize")-30)/10);
        Thermo(60,133,10,CvarInt("sensitivity")); break;
    case Sound:
        Patch("M_SVOL",60,38);
        Thermo(80,80,16,(int)(trap_Cvar_VariableValue("s_volume")*15+.5f));
        Thermo(80,112,16,(int)(trap_Cvar_VariableValue("s_musicvolume")*15+.5f)); break;
    case Load: case Save:
        Patch(screen==Load ? "M_LOADG":"M_SAVEG",72,28);
        for (int i=0;i<6;++i) {
            int y=54+i*16;
            Patch("M_LSLEFT",72,y+7);
            for (int j=0;j<24;++j) Patch("M_LSCNTR",80+j*8,y+7);
            Patch("M_LSRGHT",272,y+7);
            Text(80,y,descriptions[i]);
        }
        if (editing) Text(80+TextWidth(descriptions[selected[Save]]),54+selected[Save]*16,"_");
        break;
    default: break;
    }
    for (int i=0;i<menu.count;++i) if (menu.patches[i][0]) Patch(menu.patches[i],menu.x,menu.y+i*16);
    // Original skull toggles every eight 35 Hz tics, first frame after ten tics.
    int tics=(uiInfo.uiDC.realTime-openedAt)*35/1000;
    int frame=tics < 10 ? 0 : 1+((tics-10)/8);
    Patch((frame&1) ? "M_SKULL2":"M_SKULL1",menu.x-32,menu.y-5+selected[screen]*16);
}
void UI_DoomKey(int key,qboolean down) {
    if (!down) return;
    if (titleOnly) { if (!(key & K_CHAR_FLAG)) { titleOnly=qfalse; Play("swtchn"); } return; }
    if (prompt!=NoPrompt) {
        if (prompt!=Notice && key!='y' && key!='Y' && key!='n' && key!='N' && key!=K_ESCAPE && key!=K_SPACE) return;
        Prompt previous=prompt; prompt=NoPrompt;
        if (key=='y' || key=='Y') {
            if (previous==Quit) trap_Cmd_ExecuteText(EXEC_APPEND,"quit\n");
            if (previous==EndGame) { Close(); trap_Cmd_ExecuteText(EXEC_APPEND,"disconnect\n"); }
            if (previous==Nightmare) StartGame();
        }
        Play("swtchx"); return;
    }
    if (editingAddress) {
        const int length = static_cast<int>(strlen(serverAddress));
        if (key == K_ESCAPE) editingAddress = qfalse;
        else if (key == K_ENTER || key == K_KP_ENTER) { editingAddress = qfalse; selected[Join] = 1; }
        else if (key == K_BACKSPACE && length) serverAddress[length - 1] = 0;
        else if ((key & K_CHAR_FLAG) && length < static_cast<int>(sizeof(serverAddress)) - 1) {
            const int c = key & ~K_CHAR_FLAG;
            if (AddressCharacter(c)) { serverAddress[length] = static_cast<char>(c); serverAddress[length + 1] = 0; }
        }
        return;
    }
    if (editing) {
        char *text=descriptions[selected[Save]]; int length=strlen(text);
        if (key==K_ESCAPE) { Q_strncpyz(text,oldDescription,24); editing=qfalse; }
        else if (key==K_ENTER || key==K_KP_ENTER) SaveSlot();
        else if (key==K_BACKSPACE && length) text[length-1]=0;
        else if (key & K_CHAR_FLAG) {
            int c=toupper(key & ~K_CHAR_FLAG);
            if (c>=32 && c<=95 && length<23 && TextWidth(text)+CharWidth(c)<=176) { text[length]=c; text[length+1]=0; }
        }
        return;
    }
    if (key & K_CHAR_FLAG) return;
    switch(key) {
    case K_UPARROW: case K_KP_UPARROW: case K_MWHEELUP: Move(-1); break;
    case K_DOWNARROW: case K_KP_DOWNARROW: case K_MWHEELDOWN: Move(1); break;
    case K_LEFTARROW: case K_KP_LEFTARROW: Adjust(-1); break;
    case K_RIGHTARROW: case K_KP_RIGHTARROW: Adjust(1); break;
    case K_ENTER: case K_KP_ENTER: case K_MOUSE1: Activate(); break;
    case K_ESCAPE: case K_MOUSE2: case K_BACKSPACE:
        Play("swtchx");
        if (screen==Main) { if (inGame) Close(); else titleOnly=qtrue; }
        else if (screen==Skill) Go(Episode);
        else if (screen==Sound) Go(Options);
        else if (screen==Join) Go(Multiplayer);
        else if (screen==Credits) Go(Help);
        else Go(Main);
        break;
    case K_F1: Go(Help); break;
    case K_F2:
        if (inGame && CvarInt("g_gametype") == GT_SINGLE_PLAYER) { ReadSlots(); Go(Save); }
        else if (inGame) Message(Notice, "SAVING IS FOR SINGLE PLAYER.");
        break;
    case K_F3: ReadSlots(); Go(Load); break;
    case K_F4: Go(Sound); break;
    case K_F10: Message(Quit,"ARE YOU SURE YOU WANT TO\nQUIT THIS GREAT GAME?\n\n(PRESS Y TO QUIT)"); break;
    default:
        const Menu &menu=menus[screen];
        for (int n=1;n<=menu.count;++n) {
            int row=(selected[screen]+n)%menu.count;
            if (Selectable(row) && tolower(key)==menu.hotkeys[row]) { selected[screen]=row; Play("pstop"); break; }
        }
        break;
    }
}
void UI_DoomMouse(int dx,int dy) {
    Geometry();
    mouseX += dx/sx; mouseY += dy/sy;
    mouseX=mouseX<0 ? 0 : mouseX>319 ? 319 : mouseX;
    mouseY=mouseY<0 ? 0 : mouseY>199 ? 199 : mouseY;
    mouseTravel += (float)(abs(dx)+abs(dy));
    if (titleOnly || prompt!=NoPrompt || editing || editingAddress || mouseTravel<3) return;
    const Menu &menu=menus[screen];
    int row=(int)((mouseY-menu.y)/16);
    if (mouseY>=menu.y && row>=0 && row<menu.count && mouseX>=menu.x-32 && mouseX<310 && Selectable(row)) {
        if (selected[screen]!=row) { selected[screen]=row; Play("pstop"); }
    }
}
qboolean UI_DoomCommand(const char *command) {
    if (Q_stricmp(command,"doommenu")) return qfalse;
    char target[32]; trap_Argv(1,target,sizeof(target));
    uiClientState_t client; trap_GetClientState(&client);
    _UI_SetActiveMenu(client.connState==CA_ACTIVE ? UIMENU_INGAME : UIMENU_MAIN);
    if (!active) return qtrue;
    if (!Q_stricmp(target,"options")) Go(Options);
    else if (!Q_stricmp(target,"multiplayer")) Go(Multiplayer);
    else if (!Q_stricmp(target,"join")) Go(Join);
    else if (!Q_stricmp(target,"episode")) Go(Episode);
    else if (!Q_stricmp(target,"skill")) Go(Skill);
    else if (!Q_stricmp(target,"sound")) Go(Sound);
    else if (!Q_stricmp(target,"load")) { ReadSlots(); Go(Load); }
    else if (!Q_stricmp(target,"save")) { selected[Main]=4; Activate(); }
    else if (!Q_stricmp(target,"help")) Go(Help);
    else if (!Q_stricmp(target,"quit")) { selected[Main]=6; Activate(); }
    else if (!Q_stricmp(target,"endgame")) { Go(Options); selected[Options]=0; Activate(); }
    return qtrue;
}

qboolean UI_DoomDrawLoading() {
    char map[MAX_QPATH];
    trap_Cvar_VariableStringBuffer("ui_doomLoadingMap",map,sizeof(map));
    int ep, mission;
    if (!doomlevel::Map(map,ep,mission) || static_cast<int>(trap_Cvar_VariableValue("g_gametype")) != GT_SINGLE_PLAYER) { return qfalse; }
    Geometry();
    const vec4_t black = {0,0,0,1}; trap_R_SetColor(black);
    trap_R_DrawStretchPic(0,0,static_cast<float>(uiInfo.uiDC.glconfig.vidWidth),static_cast<float>(uiInfo.uiDC.glconfig.vidHeight),0,0,1,1,white);
    trap_R_SetColor(nullptr); Patch(va("WIMAP%d",ep-1),0,0);
    if (auto *patch = FindPatch(va("WILV%d%d",ep-1,mission-1))) { Patch(patch->name,(320-patch->width)*0.5f+patch->left,26+patch->top); }
    Text(132,140,"LOADING"); Text(92,160,"PREPARING THE LEVEL");
    return qtrue;
}
