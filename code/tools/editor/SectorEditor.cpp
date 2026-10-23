#include "SectorEditor.h"
#include "BlockWorld.h"
namespace de {
void drawSectorEditor(App &a) {
    if (!ImGui::CollapsingHeader("Sector properties", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    int first = -1, side = 0;
    for (size_t i = 0; i < a.world.lines.size(); ++i)
        if (a.world.lines[i].selected > 0) {
            first = int(i);
            side = a.world.lines[i].selected - 1;
            break;
        }
    if (first < 0) {
        ImGui::TextWrapped("Select boundary lines. Alt-click a two-sided line to select its back.");
        return;
    }
    static SectorDef edit;
    static std::string signature;
    static int previous = -1, previousSide = -1;
    auto sig = a.world.serialize();
    if (first != previous || side != previousSide || signature != sig) {
        edit = a.world.lines[first].side[side].ends;
        previous = first;
        previousSide = side;
        signature = sig;
    }
    editSector(edit);
    if (ImGui::Button("Apply to selected sides")) {
        a.world.checkpoint();
        if (!a.pickedSectorSides.empty()) {
            for (auto [line, selectedSide] : a.pickedSectorSides) {
                a.world.lines.at(line).side.at(selectedSide).ends = edit;
            }
        } else {
            for (auto &l : a.world.lines) {
                if (l.selected > 0) {
                    l.side[l.selected - 1].ends = edit;
                }
            }
        }
        a.message("Sector properties applied");
    }
    if (ImGui::Button("Select entire sector"))
        a.action([&] {
            auto copy = a.world;
            // Selection should also work when the user is repairing a region
            // whose boundary sides currently disagree about sector properties.
            auto definition = copy.lines[first].side[side].ends;
            for (auto &line : copy.lines)
                for (auto &s : line.side)
                    s.ends = definition;
            auto sectors = connectSectors(copy);
            int index = copy.lines[first].side[side].sector;
            if (index >= 0 && index < int(sectors.size())) {
                a.world.deselect();
                for (auto [line, s] : sectors[index].sides)
                    a.world.lines[line].selected = s + 1;
            }
        });
}
} // namespace de
