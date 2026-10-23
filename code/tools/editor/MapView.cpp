#include "MapView.h"
#include <algorithm>
#include <cmath>
namespace de {
ImVec2 worldToScreen(const App &a, Vec2 p) {
    return {a.view.origin.x + a.view.size.x * .5f + (p.x - a.view.center.x) * a.view.zoom,
            a.view.origin.y + a.view.size.y * .5f - (p.y - a.view.center.y) * a.view.zoom};
}
Vec2 screenToWorld(const App &a, ImVec2 p) {
    return {a.view.center.x + (p.x - a.view.origin.x - a.view.size.x * .5f) / a.view.zoom,
            a.view.center.y - (p.y - a.view.origin.y - a.view.size.y * .5f) / a.view.zoom};
}
Vec2 snapped(const App &a, Vec2 p) {
    if (!a.prefs.snap)
        return p;
    float g = float(a.prefs.grid);
    return {std::round(p.x / g) * g, std::round(p.y / g) * g};
}
void fitMap(App &a) {
    bool first = true;
    Vec2 lo{}, hi{};
    for (auto &p : a.world.points)
        if (p.selected >= 0) {
            if (first) {
                lo = hi = p.pt;
                first = false;
            }
            lo.x = std::min(lo.x, p.pt.x);
            lo.y = std::min(lo.y, p.pt.y);
            hi.x = std::max(hi.x, p.pt.x);
            hi.y = std::max(hi.y, p.pt.y);
        }
    auto include = [&](Vec2 pt) {
        if (first) {
            lo = hi = pt;
            first = false;
        }
        lo.x = std::min(lo.x, pt.x);
        lo.y = std::min(lo.y, pt.y);
        hi.x = std::max(hi.x, pt.x);
        hi.y = std::max(hi.y, pt.y);
    };
    for (const auto &t : a.world.things)
        if (t.selected >= 0)
            include(t.origin);
    for (const auto &s : a.world.thingSplines)
        for (auto pt : s.nodes)
            include(pt);
    if (first) {
        a.view.center = {};
        a.view.zoom = 1;
        return;
    }
    a.view.center = (lo + hi) * .5f;
    float width = a.view.size.x > 0 ? a.view.size.x : 900,
          height = a.view.size.y > 0 ? a.view.size.y : 600;
    a.view.zoom = std::clamp(std::min(width / std::max(128.f, hi.x - lo.x + 128),
                                      height / std::max(128.f, hi.y - lo.y + 128)),
                             .02f, 16.f);
}
} // namespace de
