#include "MapViewDraw.h"
#include <cmath>
namespace de {
void drawMap(App &a, ImDrawList *d) {
    auto &v = a.view;
    auto &p = a.prefs;
    ImVec2 end{v.origin.x + v.size.x, v.origin.y + v.size.y};
    d->PushClipRect(v.origin, end, true);
    d->AddRectFilled(v.origin, end, ImGui::ColorConvertFloat4ToU32(p.background));
    drawReferenceImage(a, d);
    if (p.showGrid) {
        float step = float(p.grid);
        while (step * v.zoom < 4)
            step *= 2;
        auto lo = screenToWorld(a, {v.origin.x, end.y}), hi = screenToWorld(a, {end.x, v.origin.y});
        for (float x = std::floor(lo.x / step) * step; x <= hi.x; x += step) {
            auto p0 = worldToScreen(a, {x, lo.y}), p1 = worldToScreen(a, {x, hi.y});
            d->AddLine(p0, p1,
                       int(x / step) % 8 == 0 ? IM_COL32(58, 69, 81, 255)
                                              : IM_COL32(29, 36, 44, 255));
        }
        for (float y = std::floor(lo.y / step) * step; y <= hi.y; y += step)
            d->AddLine(worldToScreen(a, {lo.x, y}), worldToScreen(a, {hi.x, y}),
                       int(y / step) % 8 == 0 ? IM_COL32(58, 69, 81, 255)
                                              : IM_COL32(29, 36, 44, 255));
    }
    for (auto &l : a.world.lines)
        if (l.selected >= 0) {
            auto pa = a.world.points[l.p1].pt, pb = a.world.points[l.p2].pt;
            auto x = worldToScreen(a, pa), y = worldToScreen(a, pb);
            auto color = ImGui::ColorConvertFloat4ToU32(
                l.selected > 0 ? p.selected : ((l.flags & 4) ? p.twoSided : p.oneSided));
            if (l.special && l.selected == 0)
                color = IM_COL32(0, 255, 0, 255);
            d->AddLine(x, y, color, a.dpi);
            float dx = y.x - x.x, dy = y.y - x.y, len = std::hypot(dx, dy);
            if (len > 14) {
                ImVec2 mid{(x.x + y.x) * .5f, (x.y + y.y) * .5f};
                d->AddLine(mid, {mid.x - dy / len * 7, mid.y + dx / len * 7}, color);
            }
        }
    if (p.showVertices || v.mode == 0)
        for (auto &pt : a.world.points)
            if (pt.selected >= 0) {
                auto x = worldToScreen(a, pt.pt);
                float r = 2.f * a.dpi;
                d->AddRectFilled({x.x - r, x.y - r}, {x.x + r, x.y + r},
                                 pt.selected > 0 ? ImGui::ColorConvertFloat4ToU32(p.selected)
                                                 : IM_COL32(255, 255, 0, 255));
            }
    if (p.showThings)
        for (auto &t : a.world.things)
            if (t.selected >= 0) {
                auto x = worldToScreen(a, t.origin);
                float r = std::max(2.f, 16 * v.zoom);
                auto c = t.selected > 0 ? ImGui::ColorConvertFloat4ToU32(p.selected)
                                        : IM_COL32(255, 255, 0, 255);
                d->AddRectFilled({x.x - r, x.y - r}, {x.x + r, x.y + r}, c);
                if (v.zoom < .25f && t.selected <= 0) {
                    continue;
                }
                const float angle = t.angle * 3.14159265359f / 180.f;
                const float dx = std::cos(angle), dy = -std::sin(angle);
                const float length = std::max(24.f * a.dpi, r + 14.f * a.dpi);
                const float head = 7.f * a.dpi;
                ImVec2 tip{x.x + dx * length, x.y + dy * length};
                d->AddLine(x, tip, IM_COL32(0, 0, 0, 255), 4.f * a.dpi);
                d->AddLine(x, tip, c, 2.f * a.dpi);
                d->AddTriangleFilled(
                    tip, {tip.x - dx * head - dy * head * .6f, tip.y - dy * head + dx * head * .6f},
                    {tip.x - dx * head + dy * head * .6f, tip.y - dy * head - dx * head * .6f}, c);
            }
    // Draw labels after every marker so nearby markers cannot paint over the text.
    if (p.showThings)
        for (const auto &t : a.world.things)
            if (t.selected > 0 || (t.selected == 0 && v.zoom >= .5f)) {
                const auto x = worldToScreen(a, t.origin);
                const float r = std::max(2.f, 16 * v.zoom);
                drawThingLabel(a, d, t.type, {x.x, x.y - r},
                               t.selected > 0 ? ImGui::ColorConvertFloat4ToU32(p.selected)
                                              : IM_COL32(255, 255, 0, 255));
            }
    drawThingSplines(a, d);
    if (v.drawing) {
        auto start = worldToScreen(a, v.lineStart), endp = worldToScreen(a, snapped(a, v.mouse));
        if (v.tool == 2)
            d->AddRect(start, endp, IM_COL32(255, 0, 0, 255));
        else
            d->AddLine(start, endp, IM_COL32(255, 0, 0, 255), 2);
    }
    if (v.boxing) {
        auto start = worldToScreen(a, v.boxStart), mouse = ImGui::GetIO().MousePos;
        d->AddRect(start, mouse, IM_COL32(255, 0, 0, 255));
    }
    d->PopClipRect();
}
} // namespace de
