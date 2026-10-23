#pragma once
#include "DoomProject.h"
#include "EditWorld.h"
#include "SpecialList.h"
#include "imgui.h"
#include <functional>
#include <map>
namespace de {
struct Preferences {
    int grid = 8;
    bool showGrid = true, showThings = true, showVertices = true, snap = true;
    float uiScale = 1.25f;
    ImVec4 background{0, 0, 0, 1}, oneSided{1, 1, 0, 1}, twoSided{.3f, .5f, 1, 1},
        selected{1, 1, 1, 1};
    void load();
    void save() const;
};
struct ViewState {
    Vec2 center{}, mouse{}, dragStart{}, lastDrag{}, boxStart{};
    float zoom = 1;
    int mode = 1, tool = 0;
    bool drawing = false, dragging = false, boxing = false, needsFit = true;
    Vec2 lineStart{}, lineFirst{};
    ImVec2 origin{}, size{};
};
struct ThingDef {
    std::string name, sprite;
    int angle = 0, type = 1, options = 7;
    ImVec4 color{.8f, .8f, .2f, 1};
};
struct ReferenceImage {
    std::filesystem::path path;
    ImTextureID texture = 0;
    int width = 0, height = 0;
    Vec2 center{};
    float scale = 1, opacity = .5f, rotation = 0;
};
struct App {
    EditWorld world;
    DoomProject project;
    Preferences prefs;
    ViewState view;
    ReferenceImage referenceImage;
    int activeSpline = 0, splineNode = -1;
    bool splineAdding = false, splineDragging = false;
    bool splineExtendStart = false;
    std::vector<ThingDef> thingDefs;
    SpecialList lineSpecials, sectorSpecials;
    std::vector<std::string> log;
    bool showTextures = false, showTextureEdit = false, showThings = false, showRemapper = false,
         showFind = false, showPrefs = false, showLog = false, showSpecials = false,
         showProject = false, showStripper = false, showHelp = false;
    bool quit = false, closeRequested = false, projectDirty = false;
    float dpi = 1;
    bool view3d = false, select3dTab = false, smoke3d = false;
    bool hdWallsOnly = false, hdFlatsOnly = false;
    std::vector<std::pair<int, int>> pickedSectorSides;
    int selectedTexture = -1, selectedPatch = -1, thingType = 1;
    int thingAngle = 0, thingOptions = 7;
    bool requestObjectPlacement = false;
    bool focusObjectSearch = false;
    std::string mapName = "E1M1"; // Doom export slot, independent of the document title.
    std::string documentTitle = "Untitled.dwd";
    std::function<void()> pending;
    std::string error;
    std::string status = "Ready";
    std::map<std::string, ImTextureID> imageCache;
    std::vector<ImTextureID> retiredImages;
    void *buildProcess = nullptr;
    std::filesystem::path buildDirectory, buildDestination;
    bool launchAfterBuild = false, smokeLaunch = false;
    bool navigationOnly = false;
    std::function<void(const std::string &)> messageSink, playCompiledMap;
    std::string buildMapName, buildLog;
    int buildStage = 0;
    std::filesystem::path buildWad, buildTextureDirectory;
    void startBuildStage();
    void buildNavigation(const std::filesystem::path &bsp);
    void refreshMapTextures();
    void buildMap(const std::filesystem::path &output = {}, bool launch = false);
    void launchMap();
    void launchCompiledMap();
    void pollBuild();
    void frame();
    void action(const std::function<void()> &fn);
    void guard(const std::function<void()> &fn);
    void newMap();
    void openMap();
    void saveMap(bool as = false);
    void openWad();
    void openMaterialPack();
    void discoverMaterialPacks();
    void openProject();
    void saveProject();
    void exportMap(bool legacy = false);
    void requestClose();
    void importMap(const std::string &name);
    void message(const std::string &text);
    void clearImages();
    void loadReferenceImage(const std::filesystem::path &);
    void removeReferenceImage(bool edit = true);
    void syncReferenceImage();
    ImTextureID image(const std::string &name, const std::function<Image()> &make);
};
std::filesystem::path fileDialog(bool save, const wchar_t *filter, const wchar_t *extension);
ImTextureID uploadImage(const Image &image);
void releaseImage(ImTextureID image);
void *nativeWindow();
void drawToolbar(App &);
void drawMapWindow(App &);
void drawMap(App &, ImDrawList *);
void drawReferenceImage(const App &, ImDrawList *);
void drawReferenceImageControls(App &);
void mapRespond(App &);
void placeThingAt(App &, Vec2);
void fitMap(App &);
void drawThingSplineControls(App &);
void drawThingSplines(App &, ImDrawList *);
void respondThingSpline(App &, bool hovered);
void activateThingSplineTool(App &, bool create);
ImVec2 worldToScreen(const App &, Vec2);
Vec2 screenToWorld(const App &, ImVec2);
Vec2 snapped(const App &, Vec2);
void drawLinePanel(App &);
void drawThingPanel(App &);
void drawSectorEditor(App &);
bool editSector(SectorDef &);
void drawTexturePalette(App &);
void drawTextureEdit(App &);
void drawTextureView(App &, const Texture &, float);
void drawTexturePatches(App &, Texture &);
void drawTextureNames(App &);
void drawTextureWindow(App &);
void drawFlats(App &);
void initializeThingDefinitions(App &);
void drawThingLabel(const App &, ImDrawList *, int type, ImVec2 top, ImU32 color);
void drawThingPalette(App &);
void drawThingNames(App &);
void drawThingWindow(App &);
void loadThingDefinitions(App &, const std::filesystem::path &);
void saveThingDefinitions(App &, const std::filesystem::path &);
void drawRemapper(App &);
int remapTextures(EditWorld &, const std::string &, const std::string &);
int remapFlats(EditWorld &, const std::string &, const std::string &);
int remapThings(EditWorld &, int, int);
int remapSpecials(EditWorld &, int, int);
void drawThingStripper(App &);
void drawPreferences(App &);
void drawSettings(App &);
void drawFind(App &);
void drawSpecials(App &);
void drawLog(App &);
void drawProgress(float, const char *);
bool floatingPanel(const char *title, bool *open, ImVec2 size);
void panZoom(App &);
void defaultSpecials(App &);
bool inputName(const char *label, std::string &value);
bool inputPath(const char *label, std::filesystem::path &value);
std::filesystem::path projectDirectory();
void setProjectDirectory(const std::filesystem::path &path);
} // namespace de
