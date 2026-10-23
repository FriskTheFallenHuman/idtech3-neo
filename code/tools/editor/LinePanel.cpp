#include "LinePanel.h"
#include "TextureUVEditor.h"
namespace de {
void drawLinePanel(App &a) {
    int first = -1, count = 0;
    for (size_t i = 0; i < a.world.lines.size(); ++i)
        if (a.world.lines[i].selected > 0) {
            if (first < 0)
                first = int(i);
            ++count;
        }
    if (first < 0) {
        ImGui::TextWrapped("Select lines to edit their properties.");
        if (a.view.mode == 0) {
            for (size_t i = 0; i < a.world.points.size(); ++i)
                if (a.world.points[i].selected > 0) {
                    auto pt = a.world.points[i].pt;
                    float xy[] = {pt.x, pt.y};
                    ImGui::PushID(int(i));
                    if (ImGui::InputFloat2("Vertex", xy, "%.0f",
                                           ImGuiInputTextFlags_EnterReturnsTrue)) {
                        a.world.checkpoint();
                        a.world.points[i].pt = {xy[0], xy[1]};
                    }
                    ImGui::PopID();
                }
        }
        return;
    }
    ImGui::Text("%d selected | Line %d", count, first);
    static Line edit;
    static int previous = -1;
    static std::string signature;
    auto sig = a.world.serialize();
    if (previous != first || sig != signature) {
        edit = a.world.lines[first];
        previous = first;
        signature = sig;
    }
    int side = a.world.lines[first].selected == 2 ? 1 : 0;
    ImGui::Text("Editing %s side", side ? "back" : "front");
    ImGui::InputInt("Flags", &edit.flags);
    ImGui::InputInt("Special", &edit.special);
    ImGui::InputInt("Tag", &edit.tag);
    const char *names[] = {"Impassable",     "Block monsters", "Two-sided",
                           "Upper unpegged", "Lower unpegged", "Secret",
                           "Block sound",    "Hidden",         "Mapped"};
    for (int i = 0; i < 9; ++i) {
        bool on = (edit.flags & (1 << i)) != 0;
        if (ImGui::Checkbox(names[i], &on)) {
            if (on)
                edit.flags |= 1 << i;
            else
                edit.flags &= ~(1 << i);
        }
    }
    auto &s = edit.side[side];
    inputName("Upper", s.toptexture);
    inputName("Middle", s.midtexture);
    inputName("Lower", s.bottomtexture);
    ImGui::InputInt("X offset", &s.firstcolumn);
    ImGui::InputInt("Y offset", &s.flags);
    if (ImGui::TreeNode("Wall texture UVs")) {
        editTextureUV(s.uv);
        ImGui::TreePop();
    }
    if (a.world.explicitSectors) {
        ImGui::InputInt("Sector ID", &s.sector);
        if (ImGui::Button("Assign sector to selected sides")) {
            a.world.checkpoint();
            SectorDef definition = s.ends;
            for (const auto &line : a.world.lines)
                if (line.selected >= 0)
                    for (int n = 0; n < ((line.flags & 4) ? 2 : 1); ++n)
                        if (line.side[n].sector == s.sector)
                            definition = line.side[n].ends;
            for (auto &line : a.world.lines)
                if (line.selected > 0) {
                    auto &target = line.side[line.selected - 1];
                    target.sector = s.sector;
                    target.ends = definition;
                }
        }
    }
    if (ImGui::Button("Apply line properties")) {
        a.world.checkpoint();
        for (auto &l : a.world.lines)
            if (l.selected > 0) {
                int idx = l.selected == 2 ? 1 : 0;
                l.flags = edit.flags;
                l.special = edit.special;
                l.tag = edit.tag;
                auto ends = l.side[idx].ends;
                int sector = l.side[idx].sector;
                l.side[idx] = s;
                l.side[idx].ends = ends;
                l.side[idx].sector = sector;
            }
        a.message("Line properties applied");
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip"))
        a.world.flipSelected();
}
} // namespace de
