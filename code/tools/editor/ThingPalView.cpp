#include "ThingPalView.h"
#include "ThingPalette.h"
#include "ViewportSprites.h"
#include "idfunctions.h"
#include <algorithm>
#include <cstring>
#include <iterator>
namespace de {
std::string placementObjectName(const App &a) {
    for (const auto &definition : a.thingDefs) {
        if (definition.type == a.thingType) {
            auto name = definition.name;
            std::replace(name.begin(), name.end(), '_', ' ');
            return name;
        }
    }
    return "Custom object " + std::to_string(a.thingType);
}
void choosePlacementObject(App &a, const ThingDef &definition) {
    a.thingType = definition.type;
    a.thingAngle = definition.angle;
    a.thingOptions = definition.options;
    a.requestObjectPlacement = true;
}
const char *objectCategory(int type) {
    if ((type >= 1 && type <= 4) || type == 11) {
        return "Players";
    }
    if (type >= 32000) {
        return "Custom";
    }
    switch (type) {
    case 7:
    case 9:
    case 16:
    case 58:
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
    case 71:
    case 72:
    case 84:
    case 87:
    case 88:
    case 89:
    case 3001:
    case 3002:
    case 3003:
    case 3004:
    case 3005:
    case 3006:
        return "Monsters";
    case 8:
    case 17:
    case 82:
    case 2001:
    case 2002:
    case 2003:
    case 2004:
    case 2005:
    case 2006:
    case 2007:
    case 2008:
    case 2010:
    case 2046:
    case 2047:
    case 2048:
    case 2049:
        return "Weapons & ammo";
    case 5:
    case 6:
    case 13:
    case 38:
    case 39:
    case 40:
    case 83:
    case 2011:
    case 2012:
    case 2013:
    case 2014:
    case 2015:
    case 2018:
    case 2019:
    case 2022:
    case 2023:
    case 2024:
    case 2025:
    case 2026:
    case 2045:
        return "Pickups";
    default:
        return "Props";
    }
}
void drawObjectThumbnail(App &a, const ThingDef &definition, ImVec2 size) {
    std::string prefix = definition.sprite == "-" ? "" : definition.sprite;
    if (definition.type >= 1 && definition.type <= 4) {
        prefix = "PLAYA";
    }
    if (prefix.empty()) {
        for (const auto &sprite : previewSprites) {
            if (sprite.type == definition.type) {
                prefix = sprite.frame;
                break;
            }
        }
    }
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(36, 39, 42, 255),
                        3);
    if (!prefix.empty()) {
        const auto &wad = a.project.wad;
        auto found = std::find_if(wad.lumps.begin(), wad.lumps.end(), [&](const Lump &lump) {
            return lump.name.compare(0, prefix.size(), prefix) == 0 && lump.data.size() >= 8;
        });
        if (found != wad.lumps.end()) {
            const auto name = found->name;
            const auto id = a.image("object-palette:" + name, [&] { return wad.patch(name); });
            if (id) {
                const int width = found->data[0] | (found->data[1] << 8);
                const int height = found->data[2] | (found->data[3] << 8);
                if (width > 0 && height > 0) {
                    const float scale = std::min((size.x - 4) / width, (size.y - 4) / height);
                    const ImVec2 start{origin.x + (size.x - width * scale) * .5f,
                                       origin.y + (size.y - height * scale) * .5f};
                    draw->AddImage(id, start, {start.x + width * scale, start.y + height * scale});
                    return;
                }
            }
        }
    }
    draw->AddCircle({origin.x + size.x * .5f, origin.y + size.y * .5f}, size.x * .25f,
                    IM_COL32(125, 159, 179, 255), 12, 2);
}
void drawThingNames(App &a) {
    static ImGuiTextFilter filter;
    static int category = 0;
    const char *categories[] = {"All objects", "Players", "Monsters", "Weapons & ammo",
                                "Pickups",     "Props",   "Custom"};
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##object-category", &category, categories,
                 static_cast<int>(std::size(categories)));
    ImGui::SetNextItemWidth(-1);
    if (a.focusObjectSearch) {
        ImGui::SetKeyboardFocusHere();
        a.focusObjectSearch = false;
    }
    ImGui::InputTextWithHint("##object-search", "Search name or type ID...", filter.InputBuf,
                             sizeof(filter.InputBuf), ImGuiInputTextFlags_AutoSelectAll);
    filter.Build();
    std::vector<const ThingDef *> matches;
    for (const auto &definition : a.thingDefs) {
        auto name = definition.name;
        std::replace(name.begin(), name.end(), '_', ' ');
        const auto searchable = name + " " + std::to_string(definition.type);
        if ((!category || !std::strcmp(objectCategory(definition.type), categories[category])) &&
            filter.PassFilter(searchable.c_str())) {
            matches.push_back(&definition);
        }
    }
    const auto search = upper(filter.InputBuf);
    std::sort(matches.begin(), matches.end(), [&](const ThingDef *left, const ThingDef *right) {
        auto leftName = upper(left->name), rightName = upper(right->name);
        std::replace(leftName.begin(), leftName.end(), '_', ' ');
        std::replace(rightName.begin(), rightName.end(), '_', ' ');
        const bool leftExact = leftName == search, rightExact = rightName == search;
        return leftExact != rightExact ? leftExact : leftName < rightName;
    });
    ImGui::TextDisabled("%zu objects", matches.size());
    const bool chooseFirst = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                             ImGui::GetIO().WantTextInput &&
                             ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    if (chooseFirst && !matches.empty()) {
        choosePlacementObject(a, *matches.front());
    }
    ImGui::BeginChild("Object catalog", {0, 0}, ImGuiChildFlags_Borders);
    if (matches.empty()) {
        ImGui::TextWrapped("No matching objects. Try another search or category.");
    }
    const float rowHeight = 48 * a.dpi;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(matches.size()), rowHeight + ImGui::GetStyle().ItemSpacing.y);
    while (clipper.Step()) {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            const auto &definition = *matches[index];
            ImGui::PushID(definition.type);
            const auto position = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##object", a.thingType == definition.type, 0, {0, rowHeight})) {
                choosePlacementObject(a, definition);
            }
            const auto after = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos({position.x + 3 * a.dpi, position.y + 3 * a.dpi});
            drawObjectThumbnail(a, definition, {42 * a.dpi, 42 * a.dpi});
            ImGui::SetCursorScreenPos({position.x + 52 * a.dpi, position.y + 6 * a.dpi});
            auto name = definition.name;
            std::replace(name.begin(), name.end(), '_', ' ');
            ImGui::TextUnformatted(name.c_str());
            ImGui::SetCursorScreenPos({position.x + 52 * a.dpi, position.y + 26 * a.dpi});
            ImGui::TextDisabled("%s  /  %d", objectCategory(definition.type), definition.type);
            ImGui::SetCursorScreenPos(after);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}
} // namespace de
