#include <windows.h>
#include "Coordinator.h"
#include "NodeBuild.h"
#include "BlockWorld.h"
#include "idfunctions.h"
#include <algorithm>
#include <commdlg.h>
namespace de {
std::filesystem::path fileDialog(bool save, const wchar_t *filter, const wchar_t *extension) {
    wchar_t file[32768]{};
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = static_cast<HWND>(nativeWindow());
    o.lpstrFile = file;
    o.nMaxFile = 32768;
    o.lpstrFilter = filter;
    o.lpstrDefExt = extension;
    o.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
              (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o))
        return file;
    return {};
}
void App::action(const std::function<void()> &fn) {
    try {
        fn();
    } catch (const std::exception &e) {
        error = e.what();
        message("Error: " + error);
    }
}
void App::message(const std::string &s) {
    status = s;
    log.push_back(s);
    if (log.size() > 1000)
        log.erase(log.begin());
    if (messageSink) {
        messageSink(s);
    }
}
void App::guard(const std::function<void()> &fn) {
    if (world.dirty() || projectDirty)
        pending = fn;
    else
        action(fn);
}
void App::newMap() {
    guard([this] {
        world.clear();
        removeReferenceImage();
        view = ViewState{};
        mapName = "E1M1";
        documentTitle = "Untitled.dwd";
        message("New map");
    });
}
void App::openMap() {
    guard([this] {
        auto p = fileDialog(false, L"DoomEd maps (*.dwd)\0*.dwd\0All files\0*.*\0", L"dwd");
        if (p.empty())
            return;
        world.load(p);
        syncReferenceImage();
        activeSpline = 0;
        splineNode = -1;
        splineAdding = splineDragging = false;
        mapName = upper(p.stem().u8string());
        documentTitle = p.filename().u8string();
        view.drawing = false;
        fitMap(*this);
        view.needsFit = true;
        message("Opened " + p.u8string());
    });
}
void App::saveMap(bool as) {
    action([&] {
        auto p = world.pathname;
        if (as || p.empty())
            p = fileDialog(true, L"DoomEd maps (*.dwd)\0*.dwd\0", L"dwd");
        if (p.empty())
            return;
        world.save(p);
        mapName = upper(p.stem().u8string());
        documentTitle = p.filename().u8string();
        message("Saved " + p.u8string());
    });
}
void App::openWad() {
    auto load = [this] {
        auto p = fileDialog(false, L"Doom WAD (*.wad)\0*.wad\0", L"wad");
        if (p.empty())
            return;
        project.loadWad(p);
        discoverMaterialPacks();
        clearImages();
        projectDirty = true;
        showProject = true;
        message("Loaded resources: " + p.u8string());
    };
    if (projectDirty)
        pending = load;
    else
        action(load);
}
void App::openMaterialPack() {
    action([&] {
        auto path = fileDialog(false, L"PNG material packs (*.wad)\0*.wad\0", L"wad");
        if (path.empty())
            return;
        project.loadMaterialPack(path);
        clearImages();
        projectDirty = true;
        message("Added PNG materials: " + path.u8string() +
                "; save the project to remember this pack");
    });
}
void App::discoverMaterialPacks() {
    const auto directory = projectDirectory() / "base/editor/materials";
    if (!std::filesystem::is_directory(directory))
        return;
    std::vector<std::filesystem::path> paths;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(directory))
        if (entry.is_regular_file() && upper(entry.path().extension().u8string()) == ".WAD")
            paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    const auto before = project.materialPacks.size();
    for (const auto &path : paths) {
        try {
            project.loadMaterialPack(path);
        } catch (const std::exception &error) {
            // A bad optional pack must not prevent opening a map or project.
            message("Skipped material pack " + path.filename().u8string() + ": " + error.what());
        }
    }
    if (project.materialPacks.size() != before) {
        clearImages();
        message("Loaded " + std::to_string(project.materialPacks.size() - before) +
                " material pack(s) from " + directory.u8string());
    }
}
void App::importMap(const std::string &name) {
    guard([this, name] {
        world.importMap(project.wad, name);
        removeReferenceImage();
        mapName = name;
        documentTitle = name;
        fitMap(*this);
        view.needsFit = true;
        view.drawing = false;
        message("Imported " + name + "; save as .dwd to keep edits");
    });
}
void App::openProject() {
    guard([this] {
        auto p = fileDialog(false, L"DoomEd project (*.dpr)\0*.dpr\0", L"dpr");
        if (p.empty())
            return;
        project.load(p);
        discoverMaterialPacks();
        clearImages();
        world.clear();
        removeReferenceImage();
        documentTitle = "Untitled";
        mapName = "E1M1";
        projectDirty = false;
        showProject = true;
        auto dir = p.parent_path();
        if (std::filesystem::exists(dir / "things.dsp"))
            loadThingDefinitions(*this, dir / "things.dsp");
        if (std::filesystem::exists(dir / "linespecials.dsp"))
            lineSpecials.load(dir / "linespecials.dsp");
        if (std::filesystem::exists(dir / "sectorspecials.dsp"))
            sectorSpecials.load(dir / "sectorspecials.dsp");
        message("Opened project " + p.u8string());
    });
}
void App::saveProject() {
    action([&] {
        auto p = project.pathname;
        if (p.empty())
            p = fileDialog(true, L"DoomEd project (*.dpr)\0*.dpr\0", L"dpr");
        if (p.empty())
            return;
        project.save(p);
        saveThingDefinitions(*this, p.parent_path() / "things.dsp");
        lineSpecials.save(p.parent_path() / "linespecials.dsp");
        sectorSpecials.save(p.parent_path() / "sectorspecials.dsp");
        projectDirty = false;
        message("Project saved");
    });
}
void App::exportMap(bool legacy) {
    action([&] {
        auto p = fileDialog(true, L"Doom WAD (*.wad)\0*.wad\0", L"wad");
        if (p.empty())
            return;
        require(project.wadpath.empty() || std::filesystem::weakly_canonical(p) !=
                                               std::filesystem::weakly_canonical(project.wadpath),
                "Choose a new WAD filename; the resource WAD is read-only");
        if (legacy)
            require(project.wad.materials().empty(),
                    "PNG materials require Build BSP + navigation or Editable Doom WAD export");
        if (legacy)
            world.exportLegacy(p, mapName, project.textures);
        else {
            world.exportMap(p, mapName);
            Wadfile exported;
            exported.load(p);
            exported.saveWithMaterials(p, project.wad);
        }
        message(legacy ? "Exported original 1993 map lumps"
                       : "Exported editable Doom WAD. Build BSP nodes before playing.");
    });
}
void App::requestClose() {
    closeRequested = false;
    if (buildProcess) {
        message("Please wait for the map build to finish.");
        return;
    }
    guard([this] { quit = true; });
}
bool inputName(const char *label, std::string &s) {
    char text[12]{};
    std::copy_n(s.c_str(), std::min<size_t>(11, s.size()), text);
    if (ImGui::InputText(label, text, sizeof(text),
                         ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank)) {
        s = text;
        return true;
    }
    return false;
}
bool inputPath(const char *label, std::filesystem::path &path) {
    char b[4096]{};
    auto s = path.u8string();
    std::copy_n(s.c_str(), std::min<size_t>(s.size(), 4095), b);
    if (ImGui::InputText(label, b, sizeof(b))) {
        path = std::filesystem::u8path(b);
        return true;
    }
    return false;
}
void App::frame() {
    pollBuild();
    if (closeRequested)
        requestClose();
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New map", "Ctrl+N"))
                newMap();
            if (ImGui::MenuItem("Open map...", "Ctrl+O"))
                openMap();
            if (ImGui::MenuItem("Save map", "Ctrl+S"))
                saveMap();
            if (ImGui::MenuItem("Save map as..."))
                saveMap(true);
            ImGui::Separator();
            if (ImGui::MenuItem("Open project..."))
                openProject();
            if (ImGui::MenuItem("Save project"))
                saveProject();
            if (ImGui::MenuItem("Load resource WAD..."))
                openWad();
            if (ImGui::MenuItem("Add PNG material pack..."))
                openMaterialPack();
            ImGui::Separator();
            if (ImGui::MenuItem("Build BSP + navigation...", nullptr, false, !buildProcess))
                buildMap();
            if (ImGui::MenuItem("Export editable Doom WAD..."))
                exportMap();
            if (ImGui::MenuItem("Export original 1993 WAD..."))
                exportMap(true);
            if (ImGui::MenuItem("Exit"))
                requestClose();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                world.undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y"))
                world.redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C"))
                world.copy();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                world.copy();
                world.deleteSelected();
            }
            if (ImGui::MenuItem("Paste at view center", "Ctrl+V"))
                world.paste(snapped(*this, view.center));
            if (ImGui::MenuItem("Delete", "Del"))
                world.deleteSelected();
            if (ImGui::MenuItem("Select all", "Ctrl+A"))
                world.selectAll(view.mode);
            if (ImGui::MenuItem("Deselect", "Esc"))
                world.deselect();
            if (ImGui::MenuItem("Flip lines", "F"))
                world.flipSelected();
            if (ImGui::MenuItem("Fuse coincident vertices"))
                world.fusePoints();
            if (ImGui::MenuItem("Separate selected lines"))
                world.separatePoints();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Fit map", "Home"))
                fitMap(*this);
            view.needsFit = true;
            ImGui::MenuItem("Project", nullptr, &showProject);
            ImGui::MenuItem("Textures / flats", nullptr, &showTextures);
            ImGui::MenuItem("Texture editor", nullptr, &showTextureEdit);
            ImGui::MenuItem("Thing palette", nullptr, &showThings);
            ImGui::MenuItem("Specials", nullptr, &showSpecials);
            ImGui::MenuItem("Log", nullptr, &showLog);
            ImGui::MenuItem("Preferences", nullptr, &showPrefs);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools")) {
            ImGui::MenuItem("Find line / thing", nullptr, &showFind);
            ImGui::MenuItem("Remap", nullptr, &showRemapper);
            ImGui::MenuItem("Strip things", nullptr, &showStripper);
            if (ImGui::MenuItem("Validate map"))
                action([&] {
                    auto errors = world.validate();
                    for (auto &e : errors)
                        message(e);
                    if (errors.empty()) {
                        auto copy = world;
                        auto sectors = connectSectors(copy);
                        message("Valid geometry: " + std::to_string(sectors.size()) + " sectors");
                    }
                    showLog = true;
                });
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Help"))
            showHelp = true;
        ImGui::EndMainMenuBar();
    }
    drawMapWindow(*this);
    if (showProject)
        drawSettings(*this);
    if (showTextures)
        drawTextureWindow(*this);
    if (showTextureEdit)
        drawTextureEdit(*this);
    if (showThings)
        drawThingWindow(*this);
    if (showPrefs)
        drawPreferences(*this);
    if (showRemapper)
        drawRemapper(*this);
    if (showFind)
        drawFind(*this);
    if (showSpecials)
        drawSpecials(*this);
    if (showLog)
        drawLog(*this);
    if (showStripper)
        drawThingStripper(*this);
    if (pending)
        ImGui::OpenPopup("Unsaved changes");
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save changes before continuing?");
        if (ImGui::Button("Save")) {
            if (world.dirty())
                saveMap();
            if (projectDirty)
                saveProject();
            if (!world.dirty() && !projectDirty) {
                auto fn = std::move(pending);
                pending = {};
                ImGui::CloseCurrentPopup();
                action(fn);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            auto fn = std::move(pending);
            pending = {};
            ImGui::CloseCurrentPopup();
            action(fn);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!error.empty())
        ImGui::OpenPopup("DoomEd error");
    if (ImGui::BeginPopupModal("DoomEd error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", error.c_str());
        if (ImGui::Button("OK")) {
            error.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (showHelp) {
        if (floatingPanel("DoomEd controls", &showHelp, {570, 460})) {
            ImGui::TextWrapped(
                "Select vertices, lines, or things with the toolbar. Click to select, "
                "Shift-click to add/remove, drag to move. Drag empty space to box-select. "
                "Middle-drag pans; the wheel zooms around the pointer. Home fits the map.");
            ImGui::Separator();
            ImGui::TextWrapped(
                "Draw lines: click successive endpoints, right-click or Escape finishes. Draw "
                "clockwise "
                "around rooms so the front side faces inside. Rectangle creates a clockwise room. "
                "Place "
                "thing inserts the chosen type. Press 1 over a line in the focused 2D map to "
                "insert a "
                "vertex at the pointer using the current grid snap setting. Double-click "
                "a line in vertex mode also splits it.");
            ImGui::Separator();
            ImGui::TextWrapped("Inspectors apply changes to the selected objects. Sector changes "
                               "apply to selected line "
                               "sides. Save maps in the original WorldServer v4 .dwd format. Build "
                               "BSP + navigation uses the "
                               "bundled DoomMap + BSPC node builder. Editable WAD export leaves "
                               "node building to another tool. The "
                               "original 1993 lump format is also available.");
        }
        ImGui::End();
    }
}
} // namespace de
