// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the menu against a fake engine boundary: input, drawing, files, commands.
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include "ui/ui_doom.cpp"

uiInfo_t uiInfo;
namespace test {
std::map<std::string,std::string> cvars, files;
std::map<int,std::string> handles, shaders;
std::map<int, std::string> bindings;
std::vector<std::string> draws, commands;
int catcher=0, nextHandle=1;
std::string argument;
connstate_t connection=CA_DISCONNECTED;
void Require(bool ok,const char *why) { if (!ok) throw std::runtime_error(why); }
bool Drawn(const char *name) { for (auto &s:draws) if (s==name) return true; return false; }
void Key(int key) { UI_DoomKey(key,qtrue); UI_DoomKey(key,qfalse); }
void Draw() { draws.clear(); UI_DoomDraw(); }
void Open(uiMenuCommand_t mode=UIMENU_MAIN) { _UI_SetActiveMenu(mode); }
}

int Q_stricmp(const char *a,const char *b) { return _stricmp(a,b); }
char *Q_strlwr(char *s) { for (char *p=s;*p;++p) *p=(char)tolower(*p); return s; }
void Q_strncpyz(char *d,const char *s,int n) { if (n>0) { strncpy(d,s,n-1); d[n-1]=0; } }
void QDECL Com_sprintf(char *d,int n,const char *fmt,...) {
    va_list args; va_start(args,fmt); vsnprintf(d,n,fmt,args); va_end(args);
}
char *QDECL va(const char *fmt,...) {
    static char buffers[8][4096]; static int index;
    char *result=buffers[index++%8];
    va_list args; va_start(args,fmt); vsnprintf(result,4096,fmt,args); va_end(args); return result;
}
void trap_Cvar_Register(vmCvar_t *,const char *n,const char *v,int) {
    if (!test::cvars.count(n)) test::cvars[n]=v;
}
void trap_Cvar_Set(const char *n,const char *v) { test::cvars[n]=v; }
float trap_Cvar_VariableValue(const char *n) { return (float)atof(test::cvars[n].c_str()); }
void trap_Cvar_VariableStringBuffer(const char *n,char *b,int size) { Q_strncpyz(b,test::cvars[n].c_str(),size); }
void trap_Key_SetCatcher(int value) { test::catcher=value; }
void trap_Key_GetBindingBuf(int key, c
har *b, int size) {
    Q_strncpyz(b, test::bindings[key].c_str(), size);
}
void trap_Key_SetBinding(int key, const char *binding) {
    test::bindings[key] = binding;
}
void trap_Cmd_ExecuteText(int,const char *cmd) { test::commands.push_back(cmd); }
void trap_GetClientState(uiClientState_t *state) { memset(state,0,sizeof(*state)); state->connState=test::connection; }
void trap_Argv(int,char *b,int n) { Q_strncpyz(b,test::argument.c_str(),n); }
int trap_FS_FOpenFile(const char *path,fileHandle_t *f,fsMode_t mode) {
    if (mode==FS_READ && !test::files.count(path)) { *f=0; return -1; }
    *f=test::nextHandle++; test::handles[*f]=path;
    if (mode==FS_WRITE) test::files[path].clear();
    return test::files[path].size();
}
void trap_FS_FCloseFile(fileHandle_t f) { test::handles.erase(f); }
void trap_FS_Read(void *b,int n,fileHandle_t f) { memcpy(b,test::files[test::handles[f]].data(),n); }
void trap_FS_Write(const void *b,int n,fileHandle_t f) { test::files[test::handles[f]].append((const char*)b,n); }
qhandle_t trap_R_RegisterShaderNoMip(const char *s) { int h=test::nextHandle++; test::shaders[h]=s; return h; }
void trap_R_SetColor(const float *) {}
void trap_R_DrawStretchPic(float,float,float,float,float,float,float,float,qhandle_t shader) { test::draws.push_back(test::shaders[shader]); }
sfxHandle_t trap_S_RegisterSound(const char *) { return 1; }
void trap_S_StartLocalSound(sfxHandle_t,int) {}
void trap_S_FadeAllSound(float,int) {}
void Menus_CloseAll() {}
void _UI_SetActiveMenu(uiMenuCommand_t menu) {
    UI_DoomOpen(menu);
    if (menu==UIMENU_NONE) { test::catcher=0; trap_Cvar_Set("cl_paused","0"); }
}

void trap_S_StartBackgroundTrack(const char *, const char *, int) {}

int main() {
    using namespace test;
    try {
        uiInfo.uiDC.glconfig.vidWidth=1920; uiInfo.uiDC.glconfig.vidHeight=1080;
        bindings['6'] = "custom_plasma_key";
        cvars["g_gametype"] = "2";
        UI_DoomInit(); Open(); Draw();
        Require(bindings['5'] == "weaponb
ank 5" && bindings['7'] == "weaponbank 7" &&
                    bindings['6'] == "custom_plasma_key",
                "New Doom weapon keys must retain custom bindings");
        Require(Drawn("ui/doom/titlepic") && Drawn("ui/doom/m_doom") && Drawn("ui/doom/m_ngame"),"Main must draw original title and patches");
        Require(UI_DoomFullscreen() && catcher==KEYCATCH_UI,"Title must own input and be fullscreen");
        Require(ox==240 && oy==0 && sx==4.5f && sy==5.4f,"Widescreen must retain Doom's 4:3 pixel aspect");
        Key(K_ESCAPE); Draw(); Require(Drawn("ui/doom/titlepic") && !Drawn("ui/doom/m_doom"),"Escape must dismiss title menu");
        Key(K_ENTER); Key(K_ENTER); Draw(); Require(Drawn("ui/doom/m_episod") && Drawn("ui/doom/m_epi3"),"New Game must offer compiled episodes");
        Key(K_DOWNARROW); Key(K_ENTER); Draw(); Require(Drawn("ui/doom/m_skill"),"Episode must open skill menu");
        Key(K_ENTER); Require(commands.back()=="spmap e2m1\n" && !UI_DoomActive(),"Selected episode must start and release UI");
        Require(cvars["doom_skill"]=="2" && cvars["g_gameskill"]=="1","Skill must reach game cvars");
        Open(UIMENU_INGAME); Draw();
        Require(!UI_DoomFullscreen() && !Drawn("ui/doom/titlepic") && Drawn("ui/doom/m_doom"),"Pause must overlay original menu onto gameplay");
        Require(cvars["cl_paused"]=="1","In-game menu must pause");
        Key(K_ESCAPE); Require(!UI_DoomActive() && catcher==0 && cvars["cl_paused"]=="0","Escape must resume and release input");
        Open(); Key('o'); Key(K_ENTER); cvars["sensitivity"]="4";
        // Options navigation skips the two noninteractive thermometer rows.
        Key(K_DOWNARROW); Key(K_DOWNARROW); Key(K_DOWNARROW); Key(K_DOWNARROW); Key(K_RIGHTARROW);
        Require(cvars["sensitivity"]=="5","Options arrows must adjust sensitivity and skip spacers");
        Key(K_F4); cvars["s_volume"]="1"; Key(K_RIGHTARROW);
        Require(trap_Cvar_VariableValue("s_volume")==1,"Sound slider 
must clamp at maximum");
        Key(K_LEFTARROW); Require(trap_Cvar_VariableValue("s_volume")<1,"Sound slider must update engine volume");
        Open(); Key('s'); Key(K_ENTER); Key('q'); Draw();
        Require(Drawn("ui/doom/m_doom") && commands.back()!="quit\n","Cannot save from title and notice must consume its dismissal key");
        Open(UIMENU_INGAME); Key('s'); Key(K_ENTER); Key(K_ENTER);
        for (char c:std::string("TEST SAVE")) Key(K_CHAR_FLAG|c);
        Key(K_ENTER);
        Require(commands.back()=="savegame doomslot0\n" && files["save/doomslot0.txt"]=="TEST SAVE","Save editing must persist description and invoke real engine save");
        files["save/doomslot0.svg"]="game state";
        Open(); Key('l'); Key(K_ENTER); Key(K_ENTER);
        Require(commands.back()=="loadgame doomslot0\n","Load must use matching engine slot");
        Open(); Key(K_F10); Key('n'); Require(commands.back()!="quit\n","Quit cancellation must keep process running");
        Key(K_F10); Key('y'); Require(commands.back()=="quit\n","Quit confirmation must invoke quit");
        Open(); Key('n'); Key(K_ENTER); Key(K_ENTER); Key('n'); Key(K_ENTER); Key('n');
        Require(UI_DoomActive(),"Nightmare cancellation must keep menu open");
        Key(K_ENTER); Key('y'); Require(commands.back()=="spmap e2m1\n","Nightmare confirmation must start selected episode");
        argument="save"; connection=CA_ACTIVE; Require(UI_DoomCommand("doommenu"),"Function-key menu command must be handled"); Draw();
        Require(Drawn("ui/doom/m_saveg") && cvars["cl_paused"]=="1","Function key must open appropriate in-game screen");
        Open(); selected[Main] = 0; Key(K_DOWNARROW);
        Require(selected[Main] == 1 && menus[Main].count == 7, "Multiplayer must be between New Game and Options");
        Key(K_ENTER); Require(screen == Multiplayer, "Multiplayer row must open host/join settings");
        selected[Multiplayer] = 0; Key(K_RIGHTARROW);
        Require(cvars["ui_doomMpTeam"] 
== "1", "Mode must toggle to team deathmatch");
        cvars["ui_doomMpMap"] = "13"; cvars["ui_doomMpBots"] = "5";
        selected[Multiplayer] = 6; Key(K_ENTER);
        Require(commands.back() == "map e2m5\n" && cvars["g_gametype"] == "3" &&
            cvars["doom_botCount"] == "5" && cvars["sv_maxclients"] == "16" &&
            cvars["doom_botSkill"] == "3", "Host must set compatible Doom team and bot settings");
        Open(UIMENU_INGAME);
        Require(cvars["cl_paused"] == "0", "Multiplayer menu must leave the match running");
        Go(Join); selected[Join] = 0; Key(K_ENTER);
        for (int i = 0; i < 63; ++i) Key(K_BACKSPACE);
        for (char c : std::string("127.0.0.1:27960")) Key(K_CHAR_FLAG | c);
        Key(K_CHAR_FLAG | ';'); Key(K_CHAR_FLAG | '"');
        Require(std::string(serverAddress) == "127.0.0.1:27960", "Address editor must reject command delimiters");
        Key(K_ENTER); Key(K_ENTER);
        Require(commands.back() == "connect 127.0.0.1:27960\n" && !UI_DoomActive(), "Join must connect and release input");
        Open(); Go(Join); Q_strncpyz(serverAddress, "host;quit", sizeof(serverAddress));
        const auto commandCount = commands.size(); JoinServer();
        Require(commands.size() == commandCount && prompt == Notice, "Invalid stored addresses must not execute commands");
        cvars["doom_demo"] = "1";
        cvars["ui_doomMpMap"] = "20";
        UI_DoomInit();
        Require(cvars["ui_doomMpMap"] == "8",
                "Demo must clamp saved multiplayer maps to episode 1");
        for (int lockedEpisode = 1; lockedEpisode <= 2; ++lockedEpisode) {
            Open();
            Go(Episode);
            selected[Episode] = lockedEpisode;
            Draw();
            Require(Drawn("ui/doom/m_epi2") && Drawn("ui/doom/m_epi3"),
                    "Demo must keep the registered episodes visible like original Doom");
            const auto before = commands.size();
            Key(K_ENTER);
            Require(promp
t == Notice && screen == Help && commands.size() == before &&
                        std::string(message) ==
                            "THIS IS THE SHAREWARE VERSION OF DOOM.\n\n"
                            "YOU NEED TO ORDER THE ENTIRE TRILOGY.\n\nPRESS A KEY.",
                    "Registered episodes must show original Doom's shareware notice and help");
            Key(K_ENTER);
            Draw();
            Require(Drawn("ui/doom/help1") && !Drawn("ui/doom/m_skill"),
                    "Dismissing the demo notice must show help and consume the key");
        }
        Open();
        Go(Episode);
        selected[Episode] = 0;
        Key(K_ENTER);
        Require(screen == Skill, "Demo episode 1 must open skill selection");
        selected[Skill] = 2;
        Key(K_ENTER);
        Require(commands.back() == "spmap e1m1\n", "Demo episode 1 must start normally");
        Open();
        episode = 3;
        Go(Skill);
        selected[Skill] = 4;
        Key(K_ENTER);
        const auto beforeNightmare = commands.size();
        Key('y');
        Require(commands.size() == beforeNightmare && prompt == Notice,
                "Direct skill shortcuts and nightmare confirmation cannot bypass demo mode");
        Open();
        Go(Multiplayer);
        selected[Multiplayer] = 1;
        cvars["ui_doomMpMap"] = "8";
        Key(K_RIGHTARROW);
        Require(cvars["ui_doomMpMap"] == "8", "Demo host map selector must stop at E1M9");
        cvars["ui_doomMpMap"] = "9";
        selected[Multiplayer] = 6;
        const auto beforeHost = commands.size();
        Key(K_ENTER);
        Require(commands.size() == beforeHost && prompt == Notice,
                "Stored multiplayer values cannot host a registered map in demo mode");
        Open();
        Go(Multiplayer);
        selected[Multiplayer] = 6;
        cvars["ui_doomMpMap"] = "8";
        Key(K_ENTER);
        Require(commands.back() == "map e1m9\n", "Demo must still host episode 1 multiplayer");
       
 cvars["doom_demo"] = "0";
        Open();
        Go(Episode);
        selected[Episode] = 2;
        Key(K_ENTER);
        selected[Skill] = 2;
        Key(K_ENTER);
        Require(commands.back() == "spmap e3m1\n", "Registered mode must retain episode 3 access");
        cvars["com_errorMessage"] = "engine error";
        Open();
        Require(UI_DoomActive() && prompt == Notice && std::string(message) == "engine error" &&
                    cvars["com_errorMessage"].empty(),
                "Engine errors must show a notice and clear the pending error");
        puts("Doom menu integration checks passed (title, aspect, episodes/skills, pause/resume, options, sound, save/load, prompts, shortcuts, errors).");
        return 0;
    } catch (const std::exception &e) { fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
