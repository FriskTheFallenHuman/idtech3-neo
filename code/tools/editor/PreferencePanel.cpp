#include <windows.h>
#include "PreferencePanel.h"
#include "idfunctions.h"
#include <algorithm>
#include <fstream>
#include <sstream>
namespace de {
static std::filesystem::path preferencePath() {
    wchar_t dir[32768]{};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, 32768);
    auto p = n && n < 32768 ? std::filesystem::path(dir) / L"DoomEd-Quake"
                            : std::filesystem::temp_directory_path() / L"DoomEd-Quake";
    std::filesystem::create_directories(p);
    return p / L"desktop-preferences.ini";
}
void Preferences::load() {
    std::ifstream in(preferencePath());
    std::string key;
    while (in >> key) {
        if (key == "grid")
            in >> grid;
        else if (key == "snap")
            in >> snap;
        else if (key == "showGrid")
            in >> showGrid;
        else if (key == "showThings")
            in >> showThings;
        else if (key == "showVertices")
            in >> showVertices;
        else if (key == "uiScale")
            in >> uiScale;
        else {
            ImVec4 *c = key == "background" ? &background
                        : key == "oneSided" ? &oneSided
                        : key == "twoSided" ? &twoSided
                        : key == "selected" ? &selected
                                            : nullptr;
            if (c)
                in >> c->x >> c->y >> c->z;
            else {
                std::string skip;
                std::getline(in, skip);
            }
        }
    }
    grid = std::clamp(grid, 1, 128);
    uiScale = std::clamp(uiScale, 1.f, 2.5f);
}
void Preferences::save() const {
    std::ostringstream o;
    o << "grid " << grid << "\nsnap " << snap << "\nshowGrid " << showGrid << "\nshowThings "
      << showThings << "\nshowVertices " << showVertices << "\nuiScale " << uiScale << '\n';
    auto color = [&](const char *n, ImVec4 c) {
        o << n << ' ' << c.x << ' ' << c.y << ' ' << c.z << '\n';
    };
    color("background", background);
    color("oneSided", oneSided);
    color("twoSided", twoSided);
    color("selected", selected);
    writeTextAtomic(preferencePath(), o.str());
}
void drawPreferences(App &a) {
    if (floatingPanel("Preferences", &a.showPrefs, {430, 420})) {
        auto &p = a.prefs;
        ImGui::Checkbox("Show grid", &p.showGrid);
        ImGui::Checkbox("Show things", &p.showThings);
        ImGui::Checkbox("Show vertices", &p.showVertices);
        ImGui::SliderFloat("UI scale", &p.uiScale, .75f, 2.f, "%.2f");
        ImGui::Text("Effective scale: %.0f%% (monitor DPI + UI scale)", a.dpi * 100);
        ImGui::ColorEdit3("Background", &p.background.x);
        ImGui::ColorEdit3("One-sided lines", &p.oneSided.x);
        ImGui::ColorEdit3("Two-sided lines", &p.twoSided.x);
        ImGui::ColorEdit3("Selection", &p.selected.x);
        if (ImGui::Button("Save preferences"))
            a.action([&] {
                p.save();
                a.message("Preferences saved");
            });
    }
    ImGui::End();
}
} // namespace de
