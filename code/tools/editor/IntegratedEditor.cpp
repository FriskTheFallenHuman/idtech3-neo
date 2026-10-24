// SPDX-License-Identifier: GPL-3.0-or-later
#include "../compilers/CompilerJob.h"
#include "EditorAPI.h"
#include "NextUI.h"
#include "SectorAuthoring.h"
#include "ThingPalette.h"
#include "Viewport3D.h"
#include "idfunctions.h"
#include "pathops.h"
#include "imgui_impl_sdl3.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <map>

namespace {
DoomEditorHost host{};
std::unique_ptr<de::App> app, compiler;
SDL_Window *editorWindow = nullptr;
ImGuiContext *gui = nullptr;
std::string apiError, iniPath;
bool shown = false, returnRequested = false, sdlReady = false;
bool comOwned = false;
bool ownsWindowTitle = false;
SDL_Window *titleWindow = nullptr;
std::string gameWindowTitle, lastEditorTitle;
SDL_Window *managedWindow = nullptr;
uint64_t fontTexture = 0;
int activeView = 1, requestedView = 1;
bool showOutliner = true, showOutput = false;
std::vector<de::Vec2> draft;

void configureEditorWindow() {
    const auto window = static_cast<SDL_Window *>(host.gameWindow);
    if (!window || window == managedWindow) {
        return;
    }
    managedWindow = window;
    // An editor opened from fullscreen needs a window frame too. Both modes use
    // this window and retain its size across the game/editor switch.
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
        SDL_SetWindowFullscreen(window, false);
    }
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_BORDERLESS) {
        SDL_SetWindowBordered(window, true);
    }
    // The game window allows 640x480; the editor workspace needs more room.
    // The engine puts the smaller limit back when the editor returns.
    SDL_SetWindowMinimumSize(window, 960, 640);
}

void print(const std::string &text) {
    if (host.print) {
        host.print(text.c_str());
    }
}
bool fail(const std::exception &error) {
    apiError = error.what();
    print("Editor: " + apiError);
    return false;
}
std::vector<std::pair<int, int>> selectedSides(const de::App &a) {
    if (!a.pickedSectorSides.empty()) {
        return a.pickedSectorSides;
    }
    std::vector<std::pair<int, int>> sides;
    for (size_t i = 0; i < a.world.lines.size(); ++i) {
        const auto &line = a.world.lines[i];
        if (line.selected > 0) {
            sides.emplace_back(static_cast<int>(i), line.selected - 1);
        }
    }
    return sides;
}
void returnToGame() {
    shown = false;
    returnRequested = false;
    if (ownsWindowTitle) {
        const auto window = static_cast<SDL_Window *>(host.gameWindow);
        if (window) {
            SDL_SetWindowTitle(window, gameWindowTitle.c_str());
        }
        ownsWindowTitle = false;
        titleWindow = nullptr;
        lastEditorTitle.clear();
    }
    if (gui) {
        ImGui::SetCurrentContext(gui);
        ImGui::GetIO().ClearInputKeys();
    }
    if (host.returnToGame) {
        host.returnToGame();
    }
}
void updateWindowTitle() {
    const auto window = static_cast<SDL_Window *>(host.gameWindow);
    if (!shown || !app || !window) {
        return;
    }
    if (!ownsWindowTitle) {
        // SDL hands out the title in UTF-8 and keeps the storage itself.
        gameWindowTitle = SDL_GetWindowTitle(window);
        ownsWindowTitle = true;
    }
    const auto document = app->world.pathname.empty()
                              ? std::string("Untitled.dwd")
                              : std::filesystem::absolute(app->world.pathname)
                                    .lexically_normal()
                                    .make_preferred()
                                    .u8string();
    const auto title = "DoomEdit: " + document;
    if (window != titleWindow || title != lastEditorTitle) {
        de::require(SDL_SetWindowTitle(window, title.c_str()),
                    "Cannot set DoomEdit window title");
        lastEditorTitle = title;
        titleWindow = window;
    }
}
void configureApp(de::App &value) {
    value.world.clear();
    value.messageSink = print;
    value.playCompiledMap = [](const std::string &name) {
        returnRequested = true;
        if (host.playMap) {
            host.playMap(name.c_str());
        }
    };
    de::defaultSpecials(value);
    de::initializeThingDefinitions(value);
    const auto directory = de::projectDirectory() / "base/wad";
    if (std::filesystem::is_directory(directory)) {
        std::vector<std::filesystem::path> wads;
        for (const auto &entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() &&
                de::upper(entry.path().extension().u8string()) == ".WAD") {
                wads.push_back(entry.path());
            }
        }
        std::sort(wads.begin(), wads.end());
        if (wads.size() == 1) {
            value.project.loadWad(wads.front());
        } else {
            value.message("Select a resource WAD from File before compiling");
        }
    }
}
bool validMapName(const std::string &name) {
    return !name.empty() && name.size() <= 32 &&
           name.find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
               std::string::npos;
}
void openDocument(const std::filesystem::path &path) {
    de::EditWorld next;
    next.load(path);
    if (!next.explicitSectors) {
        try {
            de::connectSectors(next);
        } catch (const std::exception &error) {
            app->message(std::string("Topology requires repair: ") + error.what());
        }
    }
    app->world = std::move(next);
    app->mapName = de::upper(path.stem().u8string());
    app->documentTitle = path.filename().u8string();
    app->pickedSectorSides.clear();
    app->activeSpline = 0;
    app->splineNode = -1;
    app->splineAdding = app->splineDragging = false;
    app->view = {};
    draft.clear();
    if (gui) {
        app->syncReferenceImage();
    }
    app->refreshMapTextures();
    app->message("Opened " + path.u8string());
}
void createUI() {
    editorWindow = static_cast<SDL_Window *>(host.gameWindow);
    if (gui) {
        ImGui::SetCurrentContext(gui);
        return;
    }
    gui = ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    iniPath = (de::projectDirectory() / "out/editor-layout.ini").u8string();
    io.IniFilename = iniPath.c_str();
    const float scale = SDL_GetWindowDisplayScale(editorWindow);
    wchar_t windowsDirectory[MAX_PATH]{};
    GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
    const auto fontPath =
        (std::filesystem::path(windowsDirectory) / "Fonts/segoeui.ttf").u8string();
    if (!io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.f * scale)) {
        io.Fonts->AddFontDefault();
    }
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.ChildRounding = 0;
    style.FrameRounding = 2;
    style.TabRounding = 2;
    style.GrabRounding = 2;
    style.FramePadding = {9, 5};
    style.WindowPadding = {10, 10};
    style.ItemSpacing = {8, 6};
    style.ItemInnerSpacing = {6, 4};
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 1;
    auto *colors = style.Colors;
    colors[ImGuiCol_Text] = {.84f, .85f, .87f, 1};
    colors[ImGuiCol_TextDisabled] = {.48f, .50f, .53f, 1};
    colors[ImGuiCol_WindowBg] = {.16f, .17f, .18f, 1};
    colors[ImGuiCol_ChildBg] = {.19f, .20f, .21f, 1};
    colors[ImGuiCol_PopupBg] = {.20f, .21f, .22f, 1};
    colors[ImGuiCol_MenuBarBg] = {.18f, .19f, .20f, 1};
    colors[ImGuiCol_Border] = {.10f, .11f, .12f, 1};
    colors[ImGuiCol_Separator] = {.12f, .13f, .14f, 1};
    colors[ImGuiCol_FrameBg] = {.13f, .14f, .15f, 1};
    colors[ImGuiCol_FrameBgHovered] = {.25f, .27f, .29f, 1};
    colors[ImGuiCol_FrameBgActive] = {.28f, .30f, .32f, 1};
    colors[ImGuiCol_Button] = {.25f, .26f, .28f, 1};
    colors[ImGuiCol_ButtonHovered] = {.33f, .35f, .38f, 1};
    colors[ImGuiCol_ButtonActive] = {.22f, .39f, .51f, 1};
    colors[ImGuiCol_Header] = {.24f, .32f, .39f, 1};
    colors[ImGuiCol_HeaderHovered] = {.30f, .39f, .47f, 1};
    colors[ImGuiCol_HeaderActive] = {.25f, .42f, .54f, 1};
    colors[ImGuiCol_Tab] = {.17f, .18f, .19f, 1};
    colors[ImGuiCol_TabHovered] = {.30f, .34f, .38f, 1};
    colors[ImGuiCol_TabSelected] = {.27f, .29f, .31f, 1};
    colors[ImGuiCol_TabSelectedOverline] = {.35f, .63f, .80f, 1};
    colors[ImGuiCol_CheckMark] = {.45f, .70f, .86f, 1};
    colors[ImGuiCol_SliderGrab] = {.42f, .58f, .69f, 1};
    colors[ImGuiCol_TableHeaderBg] = {.18f, .19f, .20f, 1};
    colors[ImGuiCol_TableBorderStrong] = {.10f, .11f, .12f, 1};
    colors[ImGuiCol_TableBorderLight] = {.14f, .15f, .16f, 1};
    style.ScaleAllSizes(scale);
    app->dpi = scale;
    sdlReady = ImGui_ImplSDL3_InitForOpenGL(editorWindow, host.glContext);
    de::require(sdlReady, "Cannot initialize editor input backend");
    app->syncReferenceImage();
}
void drawUI(const ImDrawData &data) {
    std::vector<EditorUIVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<EditorUICommand> commands;
    for (const auto *list : data.CmdLists) {
        const auto baseVertex = static_cast<uint32_t>(vertices.size());
        const auto baseIndex = static_cast<uint32_t>(indices.size());
        for (const auto &vertex : list->VtxBuffer) {
            vertices.push_back(
                {{vertex.pos.x - data.DisplayPos.x, vertex.pos.y - data.DisplayPos.y},
                 {vertex.uv.x, vertex.uv.y},
                 vertex.col});
        }
        // ImGui indices are local to a draw command's VtxOffset.
        for (int i = 0; i < list->IdxBuffer.Size; ++i) {
            indices.push_back(baseVertex + list->IdxBuffer[i]);
        }
        for (const auto &command : list->CmdBuffer) {
            de::require(command.UserCallback == nullptr, "Unsupported editor draw callback");
            for (unsigned i = 0; i < command.ElemCount; ++i) {
                indices[baseIndex + command.IdxOffset + i] += command.VtxOffset;
            }
            commands.push_back(
                {baseIndex + command.IdxOffset,
                 command.ElemCount,
                 static_cast<uint64_t>(command.GetTexID()),
                 {command.ClipRect.x - data.DisplayPos.x, command.ClipRect.y - data.DisplayPos.y,
                  command.ClipRect.z - data.DisplayPos.x, command.ClipRect.w - data.DisplayPos.y}});
        }
    }
    const EditorUIDraw draw{vertices.data(),
                            indices.data(),
                            commands.data(),
                            static_cast<uint32_t>(vertices.size()),
                            static_cast<uint32_t>(indices.size()),
                            static_cast<uint32_t>(commands.size()),
                            data.DisplaySize.x,
                            data.DisplaySize.y};
    de::require(host.renderer.drawUI(&draw), "Engine renderer rejected editor UI data");
}

void finishDraft() {
    app->action([] {
        de::Line prototype;
        const auto selected = selectedSides(*app);
        if (!selected.empty()) {
            const auto [index, side] = selected.front();
            prototype.side[0] = app->world.lines[index].side[side];
        }
        de::authorSector(app->world, draft, prototype);
        draft.clear();
        app->view.drawing = false;
        app->pickedSectorSides.clear();
        app->message("Created sector; wall direction and shared portals resolved");
    });
}
void appendDraft(de::Vec2 point) {
    if (!draft.empty() && point == draft.front() && draft.size() >= 3) {
        finishDraft();
    } else if (draft.empty() || !(point == draft.back())) {
        draft.push_back(point);
    }
}
void selectSector(de::Vec2 point, bool additive) {
    app->action([&] {
        auto sides = de::sectorAt(app->world, point);
        if (!additive) {
            app->world.deselect();
            app->pickedSectorSides.clear();
        }
        for (const auto &side : sides) {
            if (std::find(app->pickedSectorSides.begin(), app->pickedSectorSides.end(), side) ==
                app->pickedSectorSides.end()) {
                app->pickedSectorSides.push_back(side);
            }
            app->world.lines[side.first].selected = side.second + 1;
        }
    });
}
void drawCanvas() {
    auto &a = *app;
    auto &io = ImGui::GetIO();
    a.view.origin = ImGui::GetCursorScreenPos();
    a.view.size = ImGui::GetContentRegionAvail();
    a.view.size.x = std::max(1.f, a.view.size.x);
    a.view.size.y = std::max(1.f, a.view.size.y);
    if (a.view.needsFit) {
        de::fitMap(a);
        a.view.needsFit = false;
    }
    ImGui::InvisibleButton("Sector canvas", a.view.size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsMousePosValid()) {
        a.view.mouse = de::screenToWorld(a, io.MousePos);
    }
    const bool keyboard = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                          !io.WantTextInput && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    if (keyboard && !io.KeyCtrl && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && hovered) {
            a.view.tool = 1;
            appendDraft(de::snapped(a, a.view.mouse));
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) && !draft.empty()) {
            draft.pop_back();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && draft.size() >= 3) {
            finishDraft();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Insert, false) && draft.size() >= 2) {
            a.action([&] {
                de::authorDivider(a.world, draft);
                draft.clear();
                a.pickedSectorSides.clear();
                a.message("Split sector; shared wall sides resolved");
            });
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            draft.clear();
            a.view.drawing = false;
            a.view.tool = 0;
        }
        if (hovered && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            a.view.mode = 3;
            selectSector(a.view.mouse, io.KeyShift);
        }
        if (hovered && ImGui::IsKeyPressed(ImGuiKey_T, false)) {
            de::placeThingAt(a, a.view.mouse);
        }
    }
    if (a.view.tool == 1 || a.view.tool == 2) {
        if (hovered) {
            de::panZoom(a);
            if (ImGui::IsMouseClicked(1)) {
                draft.clear();
                a.view.drawing = false;
                a.view.tool = 0;
            } else if (ImGui::IsMouseClicked(0)) {
                const auto point = de::snapped(a, a.view.mouse);
                if (a.view.tool == 2 && !draft.empty()) {
                    const auto start = draft.front();
                    draft = {start, {point.x, start.y}, point, {start.x, point.y}};
                    finishDraft();
                } else {
                    appendDraft(point);
                }
            }
        }
    } else if (a.view.mode == 3) {
        if (hovered) {
            de::panZoom(a);
            if (ImGui::IsMouseClicked(0)) {
                selectSector(a.view.mouse, io.KeyShift);
            }
        }
        if (keyboard && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            a.world.undo();
            a.pickedSectorSides.clear();
        }
        if (keyboard && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            a.world.redo();
            a.pickedSectorSides.clear();
        }
    } else {
        de::mapRespond(a);
    }
    // Legacy drawing state is only a display hint; unfinished points live in draft.
    a.view.drawing = false;
    de::drawMap(a, ImGui::GetWindowDrawList());
    auto *draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(a.view.origin,
                       {a.view.origin.x + a.view.size.x, a.view.origin.y + a.view.size.y}, true);
    for (size_t i = 1; i < draft.size(); ++i) {
        draw->AddLine(de::worldToScreen(a, draft[i - 1]), de::worldToScreen(a, draft[i]),
                      IM_COL32(70, 210, 245, 255), 2 * a.dpi);
    }
    if (!draft.empty()) {
        draw->AddLine(de::worldToScreen(a, draft.back()),
                      de::worldToScreen(a, de::snapped(a, a.view.mouse)),
                      IM_COL32(70, 210, 245, 255), 2 * a.dpi);
        const auto first = de::worldToScreen(a, draft.front());
        draw->AddCircle(first, 6 * a.dpi, IM_COL32(255, 190, 80, 255), 0, 2 * a.dpi);
    }
    draw->PopClipRect();
}
void drawTextures() {
    auto &a = *app;
    static ImGuiTextFilter filter;
    filter.Draw("Find texture");
    ImGui::BeginChild("Texture catalog", {0, 175 * a.dpi}, ImGuiChildFlags_Borders);
    for (size_t i = 0; i < a.project.textures.size(); ++i) {
        const auto &texture = a.project.textures[i];
        if (filter.PassFilter(texture.name.c_str()) &&
            ImGui::Selectable(texture.name.c_str(), a.selectedTexture == static_cast<int>(i))) {
            a.selectedTexture = static_cast<int>(i);
        }
    }
    ImGui::EndChild();
    if (a.selectedTexture < 0 || a.selectedTexture >= static_cast<int>(a.project.textures.size())) {
        return;
    }
    const auto &texture = a.project.textures[a.selectedTexture];
    ImGui::Text("%s | %d x %d", texture.name.c_str(), texture.width, texture.height);
    auto id = a.image("surface:" + texture.name,
                      [&] { return a.project.wad.surfaceImage(texture.name); });
    if (id) {
        const float size = 96 * a.dpi;
        ImGui::Image(id, {size, size});
    }
    static int surface = 0;
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##surface", &surface, "Wall middle\0Wall upper\0Wall lower\0Floor\0Ceiling\0");
    if (ImGui::Button("Assign texture", {-1, 0})) {
        a.action([&] {
            de::applySurfaceTexture(a.world, selectedSides(a), texture.name, surface);
            a.message("Assigned " + texture.name);
        });
    }
}
void drawDialogs() {
    auto &a = *app;
    if (a.pending) {
        ImGui::OpenPopup("Unsaved changes");
    }
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save changes before opening another document?");
        bool proceed = false;
        if (ImGui::Button("Save")) {
            if (a.world.dirty()) {
                a.saveMap();
            }
            if (a.projectDirty) {
                a.saveProject();
            }
            proceed = !a.world.dirty() && !a.projectDirty;
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            proceed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            a.pending = {};
            ImGui::CloseCurrentPopup();
        }
        if (proceed) {
            auto action = std::move(a.pending);
            a.pending = {};
            draft.clear();
            a.pickedSectorSides.clear();
            a.action(action);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!a.error.empty()) {
        ImGui::OpenPopup("Editor error");
    }
    if (ImGui::BeginPopupModal("Editor error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(650 * a.dpi);
        ImGui::TextWrapped("%s", a.error.c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::Button("OK")) {
            a.error.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void selectMode(int mode) {
    app->world.deselect();
    app->pickedSectorSides.clear();
    app->view.mode = mode;
    app->view.tool = 0;
    app->view.drawing = false;
    draft.clear();
}
bool toolButton(int icon, const char *label, bool selected) {
    ImGui::PushID(label);
    const float scale = app->dpi;
    const auto pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("tool", {38 * scale, 38 * scale});
    const bool hovered = ImGui::IsItemHovered();
    auto *draw = ImGui::GetWindowDrawList();
    const ImU32 color = selected ? IM_COL32(128, 202, 245, 255) : IM_COL32(193, 199, 204, 255);
    draw->AddRectFilled(pos, {pos.x + 38 * scale, pos.y + 38 * scale},
                        selected  ? IM_COL32(53, 78, 96, 255)
                        : hovered ? IM_COL32(82, 87, 93, 255)
                                  : IM_COL32(55, 58, 62, 255),
                        2 * scale);
    auto point = [&](float x, float y) { return ImVec2{pos.x + x * scale, pos.y + y * scale}; };
    if (icon == 0) {
        draw->AddTriangle(point(12, 8), point(12, 29), point(18, 23), color, 1.6f * scale);
        draw->AddLine(point(18, 23), point(25, 29), color, 1.6f * scale);
        draw->AddLine(point(18, 23), point(29, 21), color, 1.6f * scale);
    } else if (icon == 1) {
        draw->AddLine(point(8, 28), point(29, 10), color, 1.6f * scale);
        for (auto p : {point(8, 28), point(29, 10)}) {
            draw->AddRectFilled({p.x - 3 * scale, p.y - 3 * scale},
                                {p.x + 3 * scale, p.y + 3 * scale}, color);
        }
    } else if (icon == 2) {
        draw->AddTriangle(point(8, 27), point(18, 9), point(30, 27), color, 1.6f * scale);
        for (auto p : {point(8, 27), point(18, 9), point(30, 27)}) {
            draw->AddCircleFilled(p, 3 * scale, color);
        }
    } else if (icon == 3) {
        draw->AddRect(point(9, 10), point(29, 28), color, 0, 0, 1.6f * scale);
        draw->AddLine(point(19, 7), point(19, 31), color, 1.2f * scale);
    } else if (icon == 4) {
        draw->AddCircle(point(19, 17), 8 * scale, color, 16, 1.6f * scale);
        draw->AddLine(point(19, 9), point(19, 26), color, 1.6f * scale);
        draw->AddLine(point(11, 17), point(27, 17), color, 1.6f * scale);
        draw->AddLine(point(27, 26), point(27, 33), color, 1.6f * scale);
        draw->AddLine(point(23, 30), point(31, 30), color, 1.6f * scale);
    } else {
        draw->AddRect(point(9, 9), point(29, 29), color, 0, 0, 1.6f * scale);
        draw->AddLine(point(15, 19), point(23, 19), color, 1.6f * scale);
        draw->AddLine(point(19, 15), point(19, 23), color, 1.6f * scale);
    }
    if (hovered) {
        ImGui::SetTooltip("%s", label);
    }
    ImGui::PopID();
    return clicked;
}
void drawOutliner() {
    auto &a = *app;
    ImGui::TextDisabled("OUTLINER");
    ImGui::Separator();
    static ImGuiTextFilter filter;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##sceneFilter", "Search scene...", filter.InputBuf,
                                 sizeof(filter.InputBuf))) {
        filter.Build();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Filter sectors and objects");
    }
    ImGui::BeginChild("Scene hierarchy", {0, -96 * a.dpi});
    ImGui::Text("%s", a.mapName.c_str());
    if (ImGui::TreeNodeEx("Sectors", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::map<int, int> sectors;
        for (const auto &line : a.world.lines) {
            if (line.selected < 0) {
                continue;
            }
            for (int side = 0; side < ((line.flags & 4) ? 2 : 1); ++side) {
                if (line.side[side].sector >= 0) {
                    ++sectors[line.side[side].sector];
                }
            }
        }
        for (const auto &[sector, count] : sectors) {
            const auto name = "Sector " + std::to_string(sector);
            if (!filter.PassFilter(name.c_str())) {
                continue;
            }
            bool selected = false;
            if (!a.pickedSectorSides.empty()) {
                const auto [line, side] = a.pickedSectorSides.front();
                selected = a.world.lines[line].side[side].sector == sector;
            }
            if (ImGui::Selectable(name.c_str(), selected)) {
                selectMode(3);
                for (size_t index = 0; index < a.world.lines.size(); ++index) {
                    auto &line = a.world.lines[index];
                    if (line.selected < 0) {
                        continue;
                    }
                    for (int side = 0; side < ((line.flags & 4) ? 2 : 1); ++side) {
                        if (line.side[side].sector == sector) {
                            a.pickedSectorSides.emplace_back(static_cast<int>(index), side);
                            line.selected = side + 1;
                        }
                    }
                }
            }
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Objects")) {
        for (size_t i = 0; i < a.world.things.size(); ++i) {
            auto &thing = a.world.things[i];
            if (thing.selected < 0) {
                continue;
            }
            std::string label = "Object " + std::to_string(i);
            for (const auto &definition : a.thingDefs) {
                if (definition.type == thing.type) {
                    label = definition.name + " " + std::to_string(i);
                    std::replace(label.begin(), label.end(), '_', ' ');
                    break;
                }
            }
            if (filter.PassFilter(label.c_str()) &&
                ImGui::Selectable(label.c_str(), thing.selected > 0)) {
                selectMode(2);
                thing.selected = 1;
            }
        }
        ImGui::TreePop();
    }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::TextDisabled("DISPLAY");
    ImGui::Checkbox("Grid", &a.prefs.showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Objects", &a.prefs.showThings);
    ImGui::Checkbox("Vertices", &a.prefs.showVertices);
}
void drawAttributes() {
    auto &a = *app;
    const auto sides = selectedSides(a);
    const bool thingSelected =
        std::any_of(a.world.things.begin(), a.world.things.end(),
                    [](const de::Thing &thing) { return thing.selected > 0; });
    if (thingSelected) {
        de::drawThingPanel(a);
    } else if (!sides.empty()) {
        if (a.view.mode == 3 || !a.pickedSectorSides.empty()) {
            de::drawSectorEditor(a);
        } else {
            de::drawLinePanel(a);
            de::drawSectorEditor(a);
        }
    } else {
        ImGui::Spacing();
        ImGui::TextUnformatted("Nothing selected");
        ImGui::TextDisabled("Select an object to edit its attributes.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("DOCUMENT");
        if (ImGui::BeginTable("Map statistics", 2, ImGuiTableFlags_SizingStretchSame)) {
            for (auto row : {std::make_pair("Format", std::string("DWD")),
                             std::make_pair("Walls", std::to_string(a.world.lines.size())),
                             std::make_pair("Objects", std::to_string(a.world.things.size()))}) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", row.first);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.second.c_str());
            }
            ImGui::EndTable();
        }
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Authoring tools")) {
        de::drawReferenceImageControls(a);
        de::drawThingSplineControls(a);
    }
}
void drawWorkspace() {
    auto &a = *app;
    if (a.requestObjectPlacement) {
        a.requestObjectPlacement = false;
        a.view.mode = 2;
        a.view.tool = 3;
        requestedView = 0;
        draft.clear();
    }
    auto &io = ImGui::GetIO();
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F12, false)) {
        returnRequested = true;
    }
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        a.saveMap(io.KeyShift);
    }
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New DWD")) {
                a.newMap();
                draft.clear();
                a.pickedSectorSides.clear();
            }
            if (ImGui::MenuItem("Open DWD...")) {
                a.openMap();
                draft.clear();
                a.pickedSectorSides.clear();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                a.saveMap();
            }
            if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S")) {
                a.saveMap(true);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Resource WAD...")) {
                a.openWad();
            }
            if (ImGui::MenuItem("PNG material pack...")) {
                a.openMaterialPack();
                a.project.textures = a.project.wad.surfaceTextures();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                a.world.undo();
                a.pickedSectorSides.clear();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                a.world.redo();
                a.pickedSectorSides.clear();
            }
            if (ImGui::MenuItem("Validate geometry")) {
                a.action([&] {
                    const auto errors = a.world.validate(false);
                    for (const auto &error : errors) {
                        a.message(error);
                    }
                    auto connected = a.world;
                    const auto sectors = de::connectSectors(connected);
                    a.message(std::to_string(sectors.size()) + " sectors; " +
                              std::to_string(errors.size()) + " validation errors");
                });
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Plan", "F2", activeView == 0)) {
                requestedView = 0;
            }
            if (ImGui::MenuItem("Perspective", "F3", activeView == 1)) {
                requestedView = 1;
            }
            ImGui::MenuItem("Outliner", nullptr, &showOutliner);
            ImGui::MenuItem("Output", nullptr, &showOutput);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Build")) {
            const bool idle = !a.buildProcess && !compiler->buildProcess;
            if (ImGui::MenuItem("Build map", nullptr, false, idle)) {
                a.buildMap();
            }
            if (ImGui::MenuItem("Build and play", nullptr, false, idle)) {
                a.buildMap({}, true);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F4, false)) {
            a.showThings = true;
            a.focusObjectSearch = true;
            a.requestObjectPlacement = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
            requestedView = 0;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
            requestedView = 1;
        }
    }
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("Doom Sector Editor", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    const float scale = a.dpi;
    // The status line groups document actions, selection and snapping. Viewports
    // occupy one tabbed panel, with independently resizable scene/attribute columns.
    if (ImGui::Button("Open")) {
        a.openMap();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        a.saveMap();
    }
    ImGui::SameLine();
    if (ImGui::Button("Place object...")) {
        a.showThings = true;
        a.focusObjectSearch = true;
        a.requestObjectPlacement = true;
    }
    ImGui::SameLine(0, 18 * scale);
    ImGui::SetNextItemWidth(100 * scale);
    int mode = a.view.mode;
    if (ImGui::Combo("##selection", &mode, "Vertices\0Walls\0Objects\0Sectors\0")) {
        selectMode(mode);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(65 * scale);
    const int grids[] = {1, 2, 4, 8, 16, 32, 64, 128};
    int grid = 0;
    for (int i = 0; i < 8; ++i) {
        if (a.prefs.grid == grids[i]) {
            grid = i;
        }
    }
    if (ImGui::Combo("##grid", &grid, "1\0 2\0 4\0 8\0 16\0 32\0 64\0 128\0")) {
        a.prefs.grid = grids[grid];
    }
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &a.prefs.snap);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::BeginDisabled(a.buildProcess || compiler->buildProcess);
    if (ImGui::Button("Build map")) {
        a.buildMap();
    }
    ImGui::SameLine();
    if (ImGui::Button("Build & play")) {
        a.buildMap({}, true);
    }
    ImGui::EndDisabled();
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 160 * scale));
    ImGui::PushStyleColor(ImGuiCol_Button, {.22f, .35f, .43f, 1});
    if (ImGui::Button("Return to game", {140 * scale, 0})) {
        returnRequested = true;
    }
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Text("%s%s", a.documentTitle.c_str(), a.world.dirty() ? " *" : "");
    ImGui::SameLine();
    ImGui::TextDisabled("   /   Object to place: %s", de::placementObjectName(a).c_str());
    ImGui::Spacing();
    const float footer = showOutput ? 150 * scale : 58 * scale;
    const float bodyHeight = std::max(120 * scale, ImGui::GetContentRegionAvail().y - footer);
    ImGui::BeginChild("Tool box", {48 * scale, bodyHeight}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPosX(5 * scale);
    if (toolButton(0, "Select / move", a.view.tool == 0)) {
        a.view.tool = 0;
        draft.clear();
    }
    ImGui::SetCursorPosX(5 * scale);
    if (toolButton(1, "Wall selection", a.view.mode == 1 && a.view.tool == 0)) {
        selectMode(1);
    }
    ImGui::Separator();
    ImGui::SetCursorPosX(5 * scale);
    if (toolButton(2, "Draw sector  |  Space", a.view.tool == 1)) {
        a.view.tool = 1;
        draft.clear();
        requestedView = 0;
    }
    ImGui::SetCursorPosX(5 * scale);
    if (toolButton(3, "Rectangle sector", a.view.tool == 2)) {
        a.view.tool = 2;
        draft.clear();
        requestedView = 0;
    }
    ImGui::SetCursorPosX(5 * scale);
    if (toolButton(4, "Place object  |  T", a.view.tool == 3)) {
        a.showThings = true;
        a.focusObjectSearch = true;
        a.requestObjectPlacement = true;
    }
    ImGui::Separator();
    ImGui::SetCursorPosX(5 * scale);
    if (toolButton(5, "Frame map  |  Home", false)) {
        a.view.needsFit = true;
        requestedView = 0;
    }
    ImGui::EndChild();
    ImGui::SameLine(0, 4 * scale);
    const int columns = showOutliner ? 3 : 2;
    if (ImGui::BeginTable("Workspace panels", columns,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingFixedFit,
                          {0, bodyHeight})) {
        if (showOutliner) {
            ImGui::TableSetupColumn("Outliner", ImGuiTableColumnFlags_WidthFixed, 180 * scale);
        }
        ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Attributes", ImGuiTableColumnFlags_WidthFixed, 300 * scale);
        ImGui::TableNextRow();
        if (showOutliner) {
            ImGui::TableNextColumn();
            ImGui::BeginChild("Outliner panel", {0, bodyHeight - 6 * scale},
                              ImGuiChildFlags_Borders);
            drawOutliner();
            ImGui::EndChild();
        }
        ImGui::TableNextColumn();
        ImGui::BeginChild("Viewport panel", {0, bodyHeight - 6 * scale}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (ImGui::BeginTabBar("Viewport tabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton |
                                                    ImGuiTabBarFlags_DrawSelectedOverline)) {
            if (ImGui::BeginTabItem("Perspective", nullptr,
                                    requestedView == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
                activeView = 1;
                de::drawViewport3D(a, ImGui::GetContentRegionAvail());
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Plan", nullptr,
                                    requestedView == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
                activeView = 0;
                ImGui::TextDisabled("Top  /  Orthographic");
                ImGui::SameLine();
                ImGui::SetCursorPosX(
                    std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 78 * scale));
                if (ImGui::SmallButton("Frame")) {
                    a.view.needsFit = true;
                }
                ImGui::Separator();
                drawCanvas();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
            requestedView = -1;
        }
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("Attribute editor", {0, bodyHeight - 6 * scale}, ImGuiChildFlags_Borders);
        ImGui::TextDisabled("ATTRIBUTE EDITOR");
        ImGui::Separator();
        if (ImGui::BeginTabBar("Attribute tabs")) {
            const auto objectFlags = a.showThings ? ImGuiTabItemFlags_SetSelected : 0;
            a.showThings = false;
            if (ImGui::BeginTabItem("Objects", nullptr, objectFlags)) {
                de::drawThingPalette(a);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Attributes")) {
                drawAttributes();
                ImGui::EndTabItem();
            }
            const auto flags = a.showTextures ? ImGuiTabItemFlags_SetSelected : 0;
            a.showTextures = false;
            if (ImGui::BeginTabItem("Textures", nullptr, flags)) {
                drawTextures();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Maps")) {
                a.action([&] {
                    std::vector<std::filesystem::path> maps;
                    for (const auto &entry : std::filesystem::directory_iterator(
                             de::projectDirectory() / "base/maps")) {
                        if (entry.is_regular_file() &&
                            de::upper(entry.path().extension().u8string()) == ".DWD") {
                            maps.push_back(entry.path());
                        }
                    }
                    std::sort(maps.begin(), maps.end());
                    for (const auto &path : maps) {
                        if (ImGui::Selectable(path.stem().u8string().c_str(),
                                              path == a.world.pathname)) {
                            a.guard([path] { openDocument(path); });
                        }
                    }
                });
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::Checkbox("Output", &showOutput);
    ImGui::SameLine();
    const auto &build = compiler->buildProcess ? *compiler : a;
    ImGui::TextDisabled("%s", build.buildProcess ? "Compiling map..." : a.status.c_str());
    if (showOutput) {
        ImGui::BeginChild("Command output", {0, 76 * scale}, ImGuiChildFlags_Borders);
        if (!build.buildLog.empty()) {
            ImGui::TextUnformatted(build.buildLog.c_str());
        } else {
            for (const auto &line : a.log) {
                ImGui::TextUnformatted(line.c_str());
            }
        }
        ImGui::EndChild();
    }
    if (a.view.tool == 3) {
        ImGui::TextDisabled(
            "Placing %s  |  Click in Plan: place  |  F4: choose object  |  Esc: stop",
            de::placementObjectName(a).c_str());
    } else {
        ImGui::TextDisabled(
            "%s",
            activeView == 0
                ? "Space: draw  |  Enter: close sector  |  Insert: split sector  |  MMB: pan  |  "
                  "Wheel: zoom"
                : "RMB + WASD: fly  |  Click: select  |  1 / 2: height  |  3 / 4: slope  |  F2: "
                  "plan");
    }
    ImGui::End();
    if (a.showPrefs) {
        de::drawPreferences(a);
    }
    drawDialogs();
}

void shutdown();
bool initialize(const DoomEditorHost *value) {
    if (!value || value->version != DoomEditorAPIVersion || value->size != sizeof(DoomEditorHost) ||
        !value->projectRoot || !value->gameWindow) {
        apiError = "Incompatible editor host interface";
        return false;
    }
    if (app) {
        return true;
    }
    try {
        host = *value;
        de::setProjectDirectory(std::filesystem::u8path(value->projectRoot));
        host.projectRoot = nullptr;
        const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        comOwned = SUCCEEDED(initialized);
        de::require(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE,
                    "Cannot initialize editor COM services");
        std::filesystem::create_directories(de::projectDirectory() / "out");
        app = std::make_unique<de::App>();
        compiler = std::make_unique<de::App>();
        configureApp(*app);
        configureApp(*compiler);
        app->showThings = true;
        app->prefs.background = {.14f, .15f, .16f, 1};
        app->prefs.oneSided = {.8f, .86f, .9f, 1};
        app->prefs.twoSided = {.25f, .65f, .8f, 1};
        app->prefs.selected = {.35f, .7f, .95f, 1};
        print("Integrated DWD editor initialized (API 3; engine renderer)");
        return true;
    } catch (const std::exception &error) {
        fail(error);
        shutdown();
        return false;
    }
}
bool show(const char *mapName) {
    try {
        de::require(app != nullptr, "Editor has not been initialized");
        if (shown && (!mapName || !*mapName)) {
            returnToGame();
            return true;
        }
        createUI();
        if (mapName && *mapName) {
            const std::string name = mapName;
            de::require(validMapName(name),
                        "editor expects a map name without a path or extension");
            const auto path = de::projectDirectory() / "base/maps" / (std::string(name) + ".dwd");
            if (path != app->world.pathname) {
                app->guard([path] { openDocument(path); });
            }
        }
        shown = true;
        configureEditorWindow();
        updateWindowTitle();
        ImGui::GetIO().AddFocusEvent(true);
        print("Editor shown; Return to game / F12 preserves the document");
        return true;
    } catch (const std::exception &error) {
        return fail(error);
    }
}
bool build(const char *mapName, bool navigationOnly) {
    try {
        de::require(app && compiler, "Editor services are unavailable");
        de::require(!app->buildProcess && !compiler->buildProcess,
                    "A map build is already running");
        if (!mapName || !*mapName) {
            de::require(!navigationOnly, "aasbuild requires a map name");
            de::require(shown, "dmap requires a map name when the editor is hidden");
            app->error.clear();
            app->buildMap();
            apiError = app->error;
            return app->buildProcess != nullptr;
        }
        std::string name = mapName;
        de::require(validMapName(name), "Expected a map name without a path or extension");
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        compiler->error.clear();
        compiler->buildLog.clear();
        const auto maps = de::projectDirectory() / "base/maps";
        if (navigationOnly) {
            compiler->buildNavigation(maps / (name + ".bsp"));
        } else {
            compiler->world.load(maps / (name + ".dwd"));
            compiler->mapName = de::upper(name);
            compiler->buildMap(maps / (name + ".bsp"));
        }
        apiError = compiler->error;
        return compiler->buildProcess != nullptr;
    } catch (const std::exception &error) {
        return fail(error);
    }
}
void frame() {
    if (!app) {
        return;
    }
    try {
        if (app->buildProcess) {
            app->pollBuild();
        }
        if (compiler->buildProcess) {
            compiler->pollBuild();
        }
        if (returnRequested) {
            returnToGame();
        }

    } catch (const std::exception &error) {
        fail(error);
        returnToGame();
    }
}
void shutdown() {
    managedWindow = nullptr;
    if (ownsWindowTitle) {
        const auto window = static_cast<SDL_Window *>(host.gameWindow);
        if (window) {
            SDL_SetWindowTitle(window, gameWindowTitle.c_str());
        }
        ownsWindowTitle = false;
        titleWindow = nullptr;
        lastEditorTitle.clear();
    }
    // Save a recovery copy on engine shutdown; ordinary window close only hides.
    // A compiler never outlives the host or publishes after the host has exited.
    for (auto *value : {app.get(), compiler.get()}) {
        if (!value) {
            continue;
        }
        if (value->buildProcess) {
            cancelCompilerJob(static_cast<CompilerJob *>(value->buildProcess));
            value->buildProcess = nullptr;
        }
    }
    if (app) {
        try {
            if (app->world.dirty()) {
                de::writeTextAtomic(de::projectDirectory() / "out/editor-recovery.dwd",
                                    app->world.serialize());
                print("Unsaved editor document recovered to out/editor-recovery.dwd");
            }
        } catch (const std::exception &error) {
            fail(error);
        }
    }
    if (gui) {
        ImGui::SetCurrentContext(gui);
        de::shutdown3D();
        if (app) {
            app->removeReferenceImage(false);
            app->clearImages();
            for (auto id : app->retiredImages) {
                de::releaseImage(id);
            }
        }
        if (fontTexture) {
            host.renderer.destroyTexture(fontTexture);
            fontTexture = 0;
        }
        if (sdlReady) {
            ImGui_ImplSDL3_Shutdown();
        }
        ImGui::DestroyContext(gui);
        gui = nullptr;
    }
    sdlReady = false;
    editorWindow = nullptr;
    app.reset();
    compiler.reset();
    draft.clear();
    shown = returnRequested = false;
    if (comOwned) {
        CoUninitialize();
        comOwned = false;
    }
}
bool visible() {
    return shown;
}
const char *lastError() {
    return apiError.c_str();
}
void resetGraphics() {
    managedWindow = nullptr;
    titleWindow = nullptr;
    if (!gui) {
        return;
    }
    ImGui::SetCurrentContext(gui);
    de::shutdown3D();
    app->removeReferenceImage(false);
    app->clearImages();
    for (auto id : app->retiredImages) {
        de::releaseImage(id);
    }
    app->retiredImages.clear();
    if (fontTexture) {
        host.renderer.destroyTexture(fontTexture);
        fontTexture = 0;
    }
    ImGui::GetIO().Fonts->SetTexID(0);
    if (sdlReady) {
        ImGui_ImplSDL3_Shutdown();
        sdlReady = false;
    }
}
void draw(int width, int height) {
    if (!shown || !app || width <= 0 || height <= 0) {
        return;
    }
    try {
        configureEditorWindow();
        createUI();
        if (!sdlReady) {
            sdlReady = ImGui_ImplSDL3_InitForOpenGL(editorWindow, host.glContext);
            de::require(sdlReady, "Cannot rebind editor input after renderer restart");
            app->syncReferenceImage();
        }
        auto &io = ImGui::GetIO();
        if (!fontTexture) {
            unsigned char *pixels = nullptr;
            int fontWidth = 0, fontHeight = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &fontWidth, &fontHeight);
            fontTexture = host.renderer.createTexture(fontWidth, fontHeight,
                                                      reinterpret_cast<const uint32_t *>(pixels));
            de::require(fontTexture != 0, "Engine renderer could not upload the editor font");
            io.Fonts->SetTexID(static_cast<ImTextureID>(fontTexture));
        }
        app->refreshMapTextures();
        ImGui_ImplSDL3_NewFrame();
        io.DisplaySize = {static_cast<float>(width), static_cast<float>(height)};
        ImGui::NewFrame();
        drawWorkspace();
        updateWindowTitle();
        ImGui::Render();
        drawUI(*ImGui::GetDrawData());
        for (auto id : app->retiredImages) {
            de::releaseImage(id);
        }
        app->retiredImages.clear();
    } catch (const std::exception &error) {
        fail(error);
        returnToGame();
    }
}
bool sdlevent(const void *value) {
    const auto *event = static_cast<const SDL_Event *>(value);
    // Keep the host window current: the engine recreates its window on
    // vid_restart and every event names the window it belongs to.
    if (const auto window = SDL_GetWindowFromEvent(event)) {
        host.gameWindow = window;
    }
    if (!shown || !gui || !sdlReady) {
        return false;
    }
    ImGui::SetCurrentContext(gui);
    ImGui_ImplSDL3_ProcessEvent(event);
    // Input belongs to the editor while it is shown; window and system
    // events still reach the engine through its own event pump.
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return true;
    default:
        return false;
    }
}
const DoomEditorAPI api{DoomEditorAPIVersion,
                        sizeof(DoomEditorAPI),
                        initialize,
                        shutdown,
                        show,
                        frame,
                        visible,
                        build,
                        lastError,
                        draw,
                        resetGraphics,
                        sdlevent};
} // namespace

namespace de {
void *nativeWindow() {
    if (!editorWindow) {
        return nullptr;
    }
    // The native file dialogs and menus want the Win32 handle behind the
    // SDL window.
    return SDL_GetPointerProperty(SDL_GetWindowProperties(editorWindow),
                                  SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
}
const EditorRenderAPI &editorRenderer() {
    return host.renderer;
}
ImTextureID uploadImage(const Image &image) {
    require(image.width > 0 && image.height > 0 &&
                image.pixels.size() == size_t(image.width) * image.height,
            "Invalid editor image dimensions or pixel count");
    const auto texture =
        host.renderer.createTexture(image.width, image.height, image.pixels.data());
    require(texture != 0, "Engine renderer could not upload an editor image");
    return static_cast<ImTextureID>(texture);
}
void releaseImage(ImTextureID id) {
    if (id) {
        host.renderer.destroyTexture(static_cast<uint64_t>(id));
    }
}
void showNativePanel(const std::string &key) {
    if (!app) {
        return;
    }
    if (key.find("Texture") != std::string::npos) {
        app->showTextures = true;
    } else if (key.find("Preference") != std::string::npos) {
        app->showPrefs = true;
    }
}
void closeNativePanel() {
    returnRequested = true;
}
void minimizeNativePanel() {
    if (editorWindow) {
        SDL_MinimizeWindow(editorWindow);
    }
}
ImFont *nextFont(const std::string &, float) {
    return ImGui::GetFont();
}
ImFont *nextFont(bool, float) {
    return ImGui::GetFont();
}
int nativeContextMenu(const std::vector<std::string> &labels, ImVec2 position) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return -1;
    }
    for (size_t i = 0; i < labels.size(); ++i) {
        AppendMenuA(menu, MF_STRING, i + 1, labels[i].c_str());
    }
    const auto hwnd = static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(editorWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) {
        DestroyMenu(menu);
        return -1;
    }
    POINT point{static_cast<LONG>(position.x), static_cast<LONG>(position.y)};
    ClientToScreen(hwnd, &point);
    const auto choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                       point.x, point.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
    return static_cast<int>(choice) - 1;
}
} // namespace de

extern "C" __declspec(dllexport) const DoomEditorAPI *__cdecl DoomEditor_GetAPI(uint32_t version) {
    return version == DoomEditorAPIVersion ? &api : nullptr;
}
