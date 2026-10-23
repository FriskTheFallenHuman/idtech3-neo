#include "FindLine.h"
namespace de {
void drawFind(App &a) {
    if (floatingPanel("Find", &a.showFind, {360, 240})) {
        static int kind = 0, number = 0;
        ImGui::Combo("Find by", &kind,
                     "Line index\0Thing index\0Line special\0Line tag\0Thing type\0");
        ImGui::InputInt("Value", &number);
        if (ImGui::Button("Find and select")) {
            a.world.deselect();
            int n = 0;
            Vec2 center{};
            if (kind == 1 || kind == 4) {
                for (size_t i = 0; i < a.world.things.size(); ++i) {
                    auto &t = a.world.things[i];
                    if (t.selected >= 0 && (kind == 1 ? int(i) == number : t.type == number)) {
                        t.selected = 1;
                        center = center + t.origin;
                        ++n;
                    }
                }
                a.view.mode = 2;
            } else {
                for (size_t i = 0; i < a.world.lines.size(); ++i) {
                    auto &l = a.world.lines[i];
                    if (l.selected >= 0 && (kind == 0   ? int(i) == number
                                            : kind == 2 ? l.special == number
                                                        : l.tag == number)) {
                        l.selected = 1;
                        center = center + (a.world.points[l.p1].pt + a.world.points[l.p2].pt) * .5f;
                        ++n;
                    }
                }
                a.view.mode = 1;
            }
            if (n)
                a.view.center = center * (1.f / n);
            a.message(std::to_string(n) + " matches");
        }
    }
    ImGui::End();
}
} // namespace de
