// Desktop controls adapted from the supplied reference editor's ToolPanel,
// LinePanel, TexturePalView and PreferencePanel. All bindings below are Doom-only.
#include "DesktopPanels.h"
#include "TextureUVEditor.h"
#include "NextUI.h"
#include "idfunctions.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace de {
namespace {
Line *selectedLine(App &a) {
    for (auto &l : a.world.lines)
        if (l.selected > 0)
            return &l;
    return nullptr;
}
Thing *selectedThing(App &a) {
    for (auto &t : a.world.things)
        if (t.selected > 0)
            return &t;
    return nullptr;
}
void at(App &a, float x, float y, float width = 0) {
    ImGui::SetCursorPos({(7 + x) * a.dpi, (24 + y) * a.dpi});
    if (width)
        ImGui::SetNextItemWidth(width * a.dpi);
}
ImVec2 sz(App &a, float w, float h) {
    return {w * a.dpi, h * a.dpi};
}
std::string &slot(Side &s, int i) {
    return i == 0 ? s.toptexture : i == 1 ? s.midtexture : s.bottomtexture;
}
void textPreview(App &a, const std::string &name, int index) {
    ImGui::PushID(index + 1000);
    at(a, 244, index * 116.f);
    const char *labels[] = {"Top", "Mid", "Bot"};
    ImGui::Text("%s: %s", labels[index], name.c_str());
    at(a, 244, index * 116.f + 17);
    auto origin = ImGui::GetCursorScreenPos();
    auto box = sz(a, 96, 88);
    bool clicked = ImGui::InvisibleButton("##preview", box);
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + box.x, origin.y + box.y}, IM_COL32(48, 48, 48, 255));
    ImTextureID image = 0;
    int width = 0, height = 0, found = -1;
    for (int i = 0; i < int(a.project.textures.size()); ++i)
        if (a.project.textures[i].name == name) {
            found = i;
            auto &t = a.project.textures[i];
            width = t.width;
            height = t.height;
            image = a.image("texture:" + name, [&] { return a.project.wad.texture(t); });
            break;
        }
    if (image && width > 0 && height > 0) {
        float fit = std::min(box.x / width, box.y / height);
        ImVec2 size{width * fit, height * fit};
        ImVec2 p{origin.x + (box.x - size.x) * .5f, origin.y + (box.y - size.y) * .5f};
        draw->AddImage(image, p, {p.x + size.x, p.y + size.y});
    } else {
        const char *message = name.empty() || name == "-"   ? "No texture"
                              : a.project.wad.lumps.empty() ? "Load WAD"
                                                            : "Not found";
        auto size = ImGui::CalcTextSize(message);
        draw->AddText({origin.x + (box.x - size.x) * .5f, origin.y + (box.y - size.y) * .5f},
                      IM_COL32(220, 220, 220, 255), message);
    }
    draw->AddRect(origin, {origin.x + box.x, origin.y + box.y}, IM_COL32(0, 0, 0, 255));
    if (clicked) {
        if (found >= 0)
            a.selectedTexture = found;
        showNativePanel("TexturePalette:Texture Palette");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s texture: %s", labels[index], name.c_str());
        if (image) {
            ImGui::Text("%d x %d", width, height);
            ImGui::TextUnformatted("Click to locate in Texture Palette.");
        }
        ImGui::EndTooltip();
    }
    ImGui::PopID();
}
void lineInspector(App &a) {
    static Line baseline;
    auto *selected = selectedLine(a);
    if (selected)
        baseline = *selected;
    int &side = desktopLineSide();
    auto &s = baseline.side[side];
    at(a, 18, 1);
    float length = 0;
    if (selected) {
        auto v = a.world.points[selected->p2].pt - a.world.points[selected->p1].pt;
        length = std::hypot(v.x, v.y);
    }
    ImGui::Text("P1: %d     P2: %d     Length: %.0f", baseline.p1, baseline.p2, length);
    const char *names[] = {"2-Sided", "Block", "No Draw",   "FTop",  "MBlock",
                           "Secret",  "FBot",  "Snd Block", "Mapped"};
    int masks[] = {4, 1, 128, 8, 2, 32, 16, 64, 256};
    for (int i = 0; i < 9; ++i) {
        at(a, 5 + (i % 3) * 77.f, 24 + (i / 3) * 16.f);
        bool on = (baseline.flags & masks[i]) != 0;
        if (ImGui::Checkbox(names[i], &on)) {
            int mask = masks[i];
            if (on)
                baseline.flags |= mask;
            else
                baseline.flags &= ~mask;
            applyDesktopLines(a, [&](Line &l) { l.flags = on ? l.flags | mask : l.flags & ~mask; });
        }
    }
    at(a, 124, 81);
    if (ImGui::RadioButton("Front", &side, 0))
        a.pickedSectorSides.clear();
    ImGui::SameLine();
    if (ImGui::RadioButton("Back", &side, 1))
        a.pickedSectorSides.clear();
    const char *labels[] = {"Row:", "Col:", "Top:", "Mid:", "Bot:"};
    for (int i = 0; i < 5; ++i) {
        at(a, 5, 82 + i * 23.f);
        ImGui::TextUnformatted(labels[i]);
        at(a, 35, 81 + i * 23.f, 81);
        ImGui::PushID(i);
        if (i < 2) {
            int &n = i == 0 ? s.flags : s.firstcolumn;
            if (ImGui::InputInt("##value", &n, 0, 0))
                applyDesktopLines(a, [&](Line &l) {
                    (i == 0 ? l.side[side].flags : l.side[side].firstcolumn) = n;
                });
        } else {
            char value[9]{};
            strncpy_s(value, slot(s, i - 2).c_str(), _TRUNCATE);
            if (ImGui::InputText("##value", value, sizeof(value),
                                 ImGuiInputTextFlags_CharsUppercase)) {
                slot(s, i - 2) = value;
                applyDesktopLines(a, [&](Line &l) { slot(l.side[side], i - 2) = value; });
            }
        }
        ImGui::PopID();
    }
    for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i + 100);
        at(a, 124, 129 + i * 23.f);
        if (ImGui::Button("Get", sz(a, 31, 21)) && a.selectedTexture >= 0 &&
            a.selectedTexture < int(a.project.textures.size())) {
            slot(s, i) = a.project.textures[a.selectedTexture].name;
            applyDesktopLines(a, [&](Line &l) { slot(l.side[side], i) = slot(s, i); });
        }
        at(a, 158, 129 + i * 23.f);
        if (ImGui::Button("Set", sz(a, 31, 21))) {
            for (int n = 0; n < int(a.project.textures.size()); ++n)
                if (a.project.textures[n].name == slot(s, i))
                    a.selectedTexture = n;
            showNativePanel("TexturePalette:Texture Palette");
        }
        at(a, 192, 129 + i * 23.f);
        if (ImGui::Button("Zero", sz(a, 31, 21))) {
            slot(s, i) = "-";
            applyDesktopLines(a, [&](Line &l) { slot(l.side[side], i) = "-"; });
        }
        ImGui::PopID();
        textPreview(a, slot(s, i), i);
    }
    at(a, 124, 105);
    if (ImGui::Button("C-Set", sz(a, 48, 21)))
        applyDesktopLines(a, [&](Line &l) { l.side[side].firstcolumn = int(length); });
    at(a, 175, 105);
    if (ImGui::Button("Calc", sz(a, 48, 21)))
        showNativePanel("line:Firstcol Calculator");
    at(a, 5, 212);
    ImGui::TextUnformatted("Special");
    at(a, 57, 210, 46);
    if (ImGui::InputInt("##special", &baseline.special, 0, 0))
        applyDesktopLines(a, [&](Line &l) { l.special = baseline.special; });
    at(a, 110, 210);
    if (ImGui::Button("Special List", sz(a, 113, 21)))
        showNativePanel("SpecialList:Special List");
    std::string name = "--- No special ---";
    for (auto &entry : a.lineSpecials.entries)
        if (entry.value == baseline.special)
            name = entry.description;
    at(a, 5, 238);
    ImGui::PushTextWrapPos(237 * a.dpi);
    ImGui::TextUnformatted(name.c_str());
    ImGui::PopTextWrapPos();
    at(a, 5, 278);
    ImGui::TextUnformatted("Tag");
    at(a, 57, 276, 46);
    if (ImGui::InputInt("##tag", &baseline.tag, 0, 0))
        applyDesktopLines(a, [&](Line &l) { l.tag = baseline.tag; });
    at(a, 110, 276);
    if (ImGui::Button("Suggest Tag", sz(a, 113, 21))) {
        int tag = 1;
        for (auto &l : a.world.lines)
            tag = std::max(tag, l.tag + 1);
        applyDesktopLines(a, [&](Line &l) { l.tag = tag; });
    }
    at(a, 5, 312);
    if (ImGui::Button("Make Passage", sz(a, 110, 21)))
        applyDesktopLines(a, [](Line &l) {
            l.flags = (l.flags | 4) & ~1;
            l.side[0].midtexture = l.side[1].midtexture = "-";
        });
    ImGui::SameLine();
    if (ImGui::Button("Flip Lines", sz(a, 103, 21)))
        a.world.flipSelected();
    at(a, 5, 341);
    if (ImGui::Button("Wall texture UVs", sz(a, 218, 21)))
        ImGui::OpenPopup("Wall UVs");
    if (ImGui::BeginPopup("Wall UVs")) {
        ImGui::Text("%s wall side (upper / middle / lower)", side ? "Back" : "Front");
        auto uv = s.uv;
        if (editTextureUV(uv))
            applyDesktopLines(a, [&](Line &l) { l.side[side].uv = uv; });
        ImGui::EndPopup();
    }
}
void tools(App &a) {
    at(a, 0, 0);
    ImGui::BeginChild("Tools content", {0, 0});
    const char *labels[] = {"Select",
                            "Poly line",
                            "Line",
                            "Zoom",
                            "Move",
                            "Sector get/fill",
                            "New thing",
                            "New thing(spline)",
                            "Edit thing spline",
                            "Launch & Save"};
    for (int i = 0; i < 10; ++i)
        if (ImGui::RadioButton(labels[i], desktopTool() == i))
            setDesktopTool(a, i);
    ImGui::Separator();
    ImGui::Text("X: %6d", int(a.view.mouse.x));
    ImGui::Text("Y: %6d", int(a.view.mouse.y));
    if (ImGui::Button("Fuse Points", {-1, 20 * a.dpi}))
        a.world.fusePoints();
    if (ImGui::Button("Separate Points", {-1, 20 * a.dpi}))
        a.world.separatePoints();
    drawThingSplineControls(a);
    ImGui::EndChild();
}
void preferences(App &a) {
    at(a, 0, 0);
    ImGui::TextUnformatted("UI size (in addition to Windows display scaling)");
    int percent = int(std::lround(a.prefs.uiScale * 100));
    ImGui::SetNextItemWidth(340 * a.dpi);
    if (ImGui::SliderInt("##scale", &percent, 100, 250, "%d%%")) {
        a.prefs.uiScale = percent / 100.f;
        a.prefs.save();
    }
    ImGui::Separator();
    bool changed = false;
    changed |= ImGui::ColorEdit3("Background", &a.prefs.background.x, ImGuiColorEditFlags_NoInputs);
    changed |= ImGui::ColorEdit3("One sided", &a.prefs.oneSided.x, ImGuiColorEditFlags_NoInputs);
    changed |= ImGui::ColorEdit3("Two sided", &a.prefs.twoSided.x, ImGuiColorEditFlags_NoInputs);
    changed |= ImGui::ColorEdit3("Selected", &a.prefs.selected.x, ImGuiColorEditFlags_NoInputs);
    changed |= ImGui::Checkbox("Show grid", &a.prefs.showGrid);
    changed |= ImGui::Checkbox("Show things", &a.prefs.showThings);
    changed |= ImGui::Checkbox("Show vertices", &a.prefs.showVertices);
    changed |= ImGui::Checkbox("Snap to grid", &a.prefs.snap);
    if (changed)
        a.prefs.save();
    ImGui::Separator();
    if (ImGui::Button("Save Preferences"))
        a.prefs.save();
    ImGui::SameLine();
    if (ImGui::Button("Defaults")) {
        a.prefs = Preferences{};
        a.prefs.save();
    }
}
void texturePalette(App &a) {
    static char filter[64]{};
    at(a, 3, 3);
    ImGui::Checkbox("HD only", &a.hdWallsOnly);
    at(a, 3, 28);
    ImGui::BeginChild("##textures", sz(a, 256, 279), ImGuiChildFlags_Borders);
    int col = 0, matches = 0;
    for (int i = 0; i < int(a.project.textures.size()); ++i) {
        auto &t = a.project.textures[i];
        if (a.hdWallsOnly && t.wadIndex != 2)
            continue;
        if (*filter && t.name.find(upper(filter)) == std::string::npos)
            continue;
        ++matches;
        if (col++)
            ImGui::SameLine();
        ImGui::PushID(i);
        ImGui::BeginGroup();
        auto id = a.image("texture:" + t.name, [&] { return a.project.wad.texture(t); });
        bool selected = a.selectedTexture == i;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{1, 1, 1, 1});
        bool hit = id ? ImGui::ImageButton("##texture", id, sz(a, 68, 68))
                      : ImGui::Button(t.name.c_str(), sz(a, 68, 68));
        if (hit)
            a.selectedTexture = i;
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            showNativePanel("TextureEdit:Texture Editor");
        if (selected)
            ImGui::PopStyleColor();
        ImGui::TextUnformatted(t.name.c_str());
        ImGui::EndGroup();
        ImGui::PopID();
        if (col == 3)
            col = 0;
    }
    if (!matches)
        ImGui::TextWrapped(
            a.project.textures.empty() ? "No wall textures loaded. Open a Doom WAD or project."
            : a.hdWallsOnly ? "No HD textures match. Add a PNG material pack or clear the search."
                            : "No textures match the search.");
    ImGui::EndChild();
    at(a, 163, 309, 94);
    ImGui::InputText("##search", filter, sizeof(filter));
    at(a, 3, 313);
    ImGui::TextUnformatted("Title:");
    if (a.selectedTexture >= 0 && a.selectedTexture < int(a.project.textures.size())) {
        auto &t = a.project.textures[a.selectedTexture];
        at(a, 38, 313);
        ImGui::TextUnformatted(t.name.c_str());
        at(a, 15, 355);
        ImGui::Text("Width   %d", t.width);
        at(a, 15, 373);
        ImGui::Text("Height  %d", t.height);
    }
    at(a, 163, 331);
    if (ImGui::Button("Clear Search", sz(a, 94, 18)))
        filter[0] = 0;
    at(a, 163, 353);
    if (ImGui::Button("Edit", sz(a, 94, 18)))
        showNativePanel("TextureEdit:Texture Editor");
    at(a, 163, 374);
    if (ImGui::Button("Show", sz(a, 94, 18)) && a.selectedTexture >= 0 &&
        a.selectedTexture < int(a.project.textures.size())) {
        auto name = a.project.textures[a.selectedTexture].name;
        a.world.deselect();
        for (auto &l : a.world.lines)
            if (l.selected >= 0)
                for (int j = 0; j < ((l.flags & 4) ? 2 : 1); ++j)
                    for (int i = 0; i < 3; ++i)
                        if (slot(l.side[j], i) == name)
                            l.selected = j + 1;
    }
}
void sectorEditor(App &a) {
    static SectorDef base;
    static char filter[64]{};
    static int plane = 0;
    if (auto *l = selectedLine(a))
        base = l->side[(l->flags & 4) ? desktopLineSide() : 0].ends;
    auto integer = [&](const char *name, float x, float y, int &value,
                       const std::function<void(SectorDef &, int)> &setter) {
        at(a, x, y, 48);
        if (ImGui::InputInt(name, &value, 0, 0))
            applyDesktopSectors(a, [&](SectorDef &s) { setter(s, value); });
    };
    at(a, 12, 23);
    ImGui::TextUnformatted("Ceiling height");
    integer("##ceiling", 30, 39, base.ceilingheight,
            [](SectorDef &s, int n) { s.ceilingheight = n; });
    at(a, 12, 73);
    ImGui::TextUnformatted(base.ceilingflat.c_str());
    at(a, 12, 83);
    ImGui::TextUnformatted("Floor height");
    integer("##floor", 30, 99, base.floorheight, [](SectorDef &s, int n) { s.floorheight = n; });
    at(a, 12, 132);
    ImGui::TextUnformatted(base.floorflat.c_str());
    at(a, 12, 150);
    ImGui::Text("Total height  %d", base.ceilingheight - base.floorheight);
    for (int i = 0; i < 4; ++i) {
        at(a, 90, 39 + (i / 2) * 60 + (i % 2) * 18);
        ImGui::PushID(i);
        if (ImGui::SmallButton(i % 2 ? "-" : "+"))
            applyDesktopSectors(a, [&](SectorDef &s) {
                (i < 2 ? s.ceilingheight : s.floorheight) += i % 2 ? -8 : 8;
            });
        ImGui::PopID();
    }
    at(a, 28, 177);
    ImGui::TextUnformatted("Light level");
    integer("##lightValue", 48, 195, base.lightlevel,
            [](SectorDef &s, int n) { s.lightlevel = std::clamp(n, 0, 255); });
    at(a, 24, 219, 90);
    if (ImGui::SliderInt("##light", &base.lightlevel, 0, 255, "", ImGuiSliderFlags_AlwaysClamp))
        applyDesktopSectors(a, [&](SectorDef &s) { s.lightlevel = base.lightlevel; });
    at(a, 127, 24);
    auto origin = ImGui::GetCursorScreenPos();
    auto size = sz(a, 128, 200);
    if (ImGui::InvisibleButton("##sector", size))
        plane = ImGui::GetIO().MousePos.y - origin.y < 105 * a.dpi ? 0 : 1;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Click the ceiling or floor preview, then choose a texture below.");
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(72, 72, 72, 255));
    for (int i = 0; i < 2; ++i) {
        auto name = i ? base.floorflat : base.ceilingflat;
        auto id = a.image("flat:" + name, [&] { return a.project.wad.flat(name); });
        if (id)
            draw->AddImage(id, {origin.x + 32 * a.dpi, origin.y + (31 + i * 74) * a.dpi},
                           {origin.x + 96 * a.dpi, origin.y + (95 + i * 74) * a.dpi});
        if (plane == i)
            draw->AddRect({origin.x + 31 * a.dpi, origin.y + (30 + i * 74) * a.dpi},
                          {origin.x + 97 * a.dpi, origin.y + (96 + i * 74) * a.dpi},
                          IM_COL32(255, 220, 96, 255), 0, 0, 2 * a.dpi);
    }
    at(a, 12, 235);
    ImGui::TextUnformatted("Special");
    at(a, 72, 235);
    ImGui::TextUnformatted("Tag");
    integer("##special", 12, 251, base.special, [](SectorDef &s, int n) { s.special = n; });
    integer("##tag", 72, 251, base.tag, [](SectorDef &s, int n) { s.tag = n; });
    at(a, 129, 251);
    if (ImGui::Button("Special List", sz(a, 127, 20)))
        showNativePanel("SpecialList:Sector Specials");
    at(a, 12, 276);
    if (ImGui::Button("Get Tag", sz(a, 106, 20))) {
        int n = 1;
        for (auto &l : a.world.lines)
            for (auto &side : l.side)
                n = std::max(n, side.ends.tag + 1);
        applyDesktopSectors(a, [&](SectorDef &s) { s.tag = n; });
    }
    at(a, 130, 276);
    if (ImGui::Button("Tag LINE search", sz(a, 127, 20))) {
        a.world.deselect();
        for (auto &l : a.world.lines)
            if (l.selected >= 0 && l.tag == base.tag)
                l.selected = 1;
    }
    at(a, 12, 300);
    if (ImGui::Button("Tag SECTOR search", sz(a, 145, 18))) {
        a.world.deselect();
        for (auto &l : a.world.lines)
            if (l.selected >= 0)
                for (int j = 0; j < ((l.flags & 4) ? 2 : 1); ++j)
                    if (l.side[j].ends.tag == base.tag)
                        l.selected = j + 1;
    }
    at(a, 11, 324);
    ImGui::RadioButton("Ceiling", &plane, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Floor", &plane, 1);
    at(a, 173, 324, 90);
    ImGui::InputText("##filter", filter, sizeof(filter));
    at(a, 12, 349);
    ImGui::TextUnformatted("Texture rotation (deg)");
    at(a, 167, 346, 90);
    float rotation = (plane ? base.rotation.floor : base.rotation.ceiling) / 1000.f;
    ImGui::PushID(plane);
    if (ImGui::InputFloat("##rotation", &rotation, 0, 0, "%.3f"))
        applyDesktopSectors(a, [&](SectorDef &s) {
            (plane ? s.rotation.floor : s.rotation.ceiling) = textureMillidegrees(rotation);
        });
    ImGui::PopID();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Rotates only the checked surface's texture UVs; geometry and slope stay fixed.");
    at(a, 8, 375);
    ImGui::Checkbox("HD only", &a.hdFlatsOnly);
    at(a, 116, 374);
    if (ImGui::Button("Texture UVs", sz(a, 140, 21)))
        ImGui::OpenPopup("Plane UVs");
    if (ImGui::BeginPopup("Plane UVs")) {
        ImGui::Text("%s texture", plane ? "Floor" : "Ceiling");
        auto uv = plane ? base.floorUV : base.ceilingUV;
        if (editTextureUV(uv))
            applyDesktopSectors(a, [&](SectorDef &s) { (plane ? s.floorUV : s.ceilingUV) = uv; });
        ImGui::EndPopup();
    }
    at(a, 8, 400);
    ImGui::BeginChild("##flats",
                      {256 * a.dpi, std::max(40 * a.dpi, ImGui::GetWindowHeight() - 435 * a.dpi)},
                      ImGuiChildFlags_Borders);
    int col = 0, matches = 0;
    for (auto &name : a.project.wad.flatNames(a.hdFlatsOnly)) {
        if (*filter && name.find(upper(filter)) == std::string::npos)
            continue;
        ++matches;
        if (col++)
            ImGui::SameLine();
        ImGui::PushID(name.c_str());
        auto id = a.image("flat:" + name, [&] { return a.project.wad.flat(name); });
        if (id && ImGui::ImageButton("##flat", id, sz(a, 64, 64)))
            applyDesktopSectors(
                a, [&](SectorDef &s) { (plane ? s.floorflat : s.ceilingflat) = name; });
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", name.c_str());
        ImGui::PopID();
        if (col == 3)
            col = 0;
    }
    if (!matches)
        ImGui::TextWrapped(a.hdFlatsOnly
                               ? "No HD flats match. Add a PNG material pack or clear the search."
                               : "No flats match the search.");
    ImGui::EndChild();
}
void project(App &a) {
    at(a, 0, 0);
    ImGui::TextUnformatted("Doom project");
    ImGui::TextWrapped("%s", a.project.pathname.u8string().c_str());
    if (ImGui::Button("Open Project..."))
        a.openProject();
    ImGui::SameLine();
    if (ImGui::Button("Save Project"))
        a.saveProject();
    ImGui::Separator();
    ImGui::TextWrapped("WAD: %s", a.project.wadpath.u8string().c_str());
    if (ImGui::Button("Load Doom WAD..."))
        a.openWad();
    if (ImGui::Button("Add PNG material pack..."))
        a.openMaterialPack();
    ImGui::Text("PNG materials: %zu", a.project.wad.materials().size());
    ImGui::TextWrapped("Map textures: %s", a.project.wad.textureDirectory.u8string().c_str());
    if (ImGui::Button("Reload map textures")) {
        a.project.wad.textureDirectory.clear();
        a.refreshMapTextures();
    }
    ImGui::Separator();
    char slot[9]{};
    strncpy(slot, a.mapName.c_str(), 8);
    ImGui::SetNextItemWidth(100 * a.dpi);
    if (ImGui::InputText("Map slot", slot, sizeof(slot), ImGuiInputTextFlags_CharsUppercase))
        a.mapName = slot;
    ImGui::TextUnformatted("Maps");
    ImGui::BeginChild("maps", {0, 210 * a.dpi}, ImGuiChildFlags_Borders);
    for (auto &name : a.project.wad.mapNames())
        if (ImGui::Selectable(name.c_str(), a.mapName == name)) {
            a.importMap(name);
            showNativePanel("Map");
        }
    ImGui::EndChild();
    if (ImGui::Button("Open DWD..."))
        a.openMap();
    ImGui::SameLine();
    if (ImGui::Button("New Map"))
        a.newMap();
    ImGui::SameLine();
    if (ImGui::Button("Build BSP + navigation..."))
        a.buildMap();
    if (ImGui::Button("Launch"))
        a.launchMap();
}
} // namespace
bool drawDesktopPanel(App &a, const std::string &key) {
    if (key == "line:Line Inspector")
        lineInspector(a);
    else if (key == "DoomEd:TOOLS")
        tools(a);
    else if (key == "preferences:Preferences")
        preferences(a);
    else if (key == "TexturePalette:Texture Palette")
        texturePalette(a);
    else if (key == "Project:Project Inspector")
        project(a);
    else if (key == "SectorEditor:Sector Editor")
        sectorEditor(a);
    else
        return false;
    return true;
}
} // namespace de
