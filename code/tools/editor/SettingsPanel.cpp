#include "SettingsPanel.h"
#include "idfunctions.h"
#include <algorithm>
namespace de {
void drawSettings(App &a) {
    if (floatingPanel("Project", &a.showProject, {520, 640})) {
        ImGui::TextWrapped("%s", a.project.pathname.empty()
                                     ? "Unsaved project"
                                     : a.project.pathname.u8string().c_str());
        ImGui::TextWrapped("WAD: %s", a.project.wadpath.u8string().c_str());
        if (ImGui::Button("Load WAD..."))
            a.openWad();
        ImGui::SameLine();
        if (ImGui::Button("Save project"))
            a.saveProject();
        ImGui::TextUnformatted("Compilers: dmap + aasbuild (toolsx86.dll)");
        inputName("Export map marker", a.mapName);
        ImGui::SeparatorText("WAD maps");
        ImGui::BeginChild("WAD maps", {0, 160 * a.dpi}, ImGuiChildFlags_Borders);
        for (auto &n : a.project.wad.mapNames())
            if (ImGui::Selectable(n.c_str(), n == a.mapName))
                a.importMap(n);
        ImGui::EndChild();
        ImGui::SeparatorText("Project maps");
        ImGui::BeginChild("Project maps", {0, 140 * a.dpi}, ImGuiChildFlags_Borders);
        for (auto &n : a.project.maps) {
            if (ImGui::Selectable(n.c_str(), n == a.mapName)) {
                auto path = a.project.pathname.parent_path() / (n + ".dwd");
                a.guard([&a, path, n] {
                    a.world.load(path);
                    a.mapName = n;
                    fitMap(a);
                });
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("Add current map")) {
            a.action([&] {
                require(!a.project.pathname.empty(), "Save the project first");
                require(!a.mapName.empty() && a.mapName.size() <= 8,
                        "Map names must contain 1 to 8 characters");
                auto p = a.project.pathname.parent_path() / (a.mapName + ".dwd");
                require(!std::filesystem::exists(p) || p == a.world.pathname,
                        "A different map already uses this filename");
                a.world.save(p);
                if (std::find(a.project.maps.begin(), a.project.maps.end(), a.mapName) ==
                    a.project.maps.end())
                    a.project.maps.push_back(a.mapName);
                a.projectDirty = true;
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove from project")) {
            auto &maps = a.project.maps;
            maps.erase(std::remove(maps.begin(), maps.end(), a.mapName), maps.end());
            a.projectDirty = true;
        }
        ImGui::TextWrapped("File > Build BSP + navigation runs DoomMap followed by all four "
                           "BSPC navigation profiles.");
    }
    ImGui::End();
}
} // namespace de
