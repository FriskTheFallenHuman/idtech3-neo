#include "FlatsView.h"
namespace de {
void drawFlats(App &a) {
    static std::string selected;
    static ImGuiTextFilter filter;
    filter.Draw("Filter flats");
    ImGui::Checkbox("HD only", &a.hdFlatsOnly);
    ImGui::BeginChild("Flat names", {0, 230 * a.dpi}, ImGuiChildFlags_Borders);
    for (auto &n : a.project.wad.flatNames(a.hdFlatsOnly))
        if (filter.PassFilter(n.c_str()) && ImGui::Selectable(n.c_str(), n == selected))
            selected = n;
    ImGui::EndChild();
    if (selected.empty())
        return;
    auto id = a.image("flat:" + selected, [&] { return a.project.wad.flat(selected); });
    if (id)
        ImGui::Image(id, {192 * a.dpi, 192 * a.dpi});
    for (int side = 0; side < 2; ++side) {
        if (side)
            ImGui::SameLine();
        if (ImGui::Button(side ? "Apply ceiling" : "Apply floor")) {
            a.world.checkpoint();
            for (auto &l : a.world.lines)
                if (l.selected > 0) {
                    auto &e = l.side[l.selected - 1].ends;
                    if (side)
                        e.ceilingflat = selected;
                    else
                        e.floorflat = selected;
                }
        }
    }
}
} // namespace de
