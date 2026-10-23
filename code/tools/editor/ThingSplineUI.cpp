#include "UI.h"
#include "ThingSpline.h"
#include "ModelProps.h"
#include "NextUI.h"
#include "DesktopPanels.h"
#include "pathops.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cfloat>
namespace de {
namespace {
float meshSpacing(int type, float &offset) {
    const auto *def = modelProps().find(type);
    if (!def)
        return 32;
    const auto &model = modelProps().load(type);
    float loX = FLT_MAX, hiX = -FLT_MAX, loY = FLT_MAX, hiY = -FLT_MAX;
    for (const auto &s : model.surfaces)
        for (const auto &v : s.frames[def->first]) {
            loX = std::min(loX, v.position.x);
            hiX = std::max(hiX, v.position.x);
            loY = std::min(loY, v.position.y);
            hiY = std::max(hiY, v.position.y);
        }
    if (hiY - loY > hiX - loX)
        offset = -90;
    return std::clamp(std::max(hiX - loX, hiY - loY) * def->scale, 1.f, 65536.f);
}
void changeSpline(App &a, ThingSpline next) {
    auto generated = generateSplineThings(next);
    a.world.checkpoint();
    *a.world.findSpline(next.id) = std::move(next);
    a.world.rebuildSpline(a.activeSpline);
}
void removeNode(App &a) {
    auto *s = a.world.findSpline(a.activeSpline);
    if (!s || a.splineNode < 0 || a.splineNode >= int(s->nodes.size()))
        return;
    a.world.checkpoint();
    if (s->nodes.size() == 1) {
        a.world.deleteSpline(s->id);
        a.activeSpline = 0;
    } else {
        s->nodes.erase(s->nodes.begin() + a.splineNode);
        a.world.rebuildSpline(s->id);
    }
    a.splineNode = -1;
    a.splineDragging = false;
}
} // namespace
void activateThingSplineTool(App &a, bool create) {
    a.view3d = false;
    a.select3dTab = true;
    a.view.drawing = a.view.dragging = a.view.boxing = false;
    a.splineDragging = false;
    a.splineNode = -1;
    a.splineAdding = create;
    a.splineExtendStart = false;
    if (create)
        a.activeSpline = 0;
    a.message(create ? "Click to start a thing spline; click to add nodes, drag to move, "
                       "Shift-click curve to insert, Delete to remove; right-click finishes"
                     : "Click a spline or node to edit; drag nodes, Shift-click curve to insert, "
                       "Delete to remove");
}
void drawThingSplineControls(App &a) {
    if (desktopTool() != 7 && desktopTool() != 8)
        return;
    ImGui::Separator();
    auto *s = a.world.findSpline(a.activeSpline);
    if (!s) {
        a.activeSpline = 0;
        a.splineNode = -1;
    }
    if (ImGui::Button("Thing Palette", {-1, 0}))
        showNativePanel("ThingPalette:Thing Palette");
    if (ImGui::BeginCombo("##spline",
                          s ? ("Spline " + std::to_string(s->id)).c_str() : "Choose spline")) {
        for (const auto &candidate : a.world.thingSplines)
            if (ImGui::Selectable(("Spline " + std::to_string(candidate.id) + " (" +
                                   std::to_string(candidate.type) + ")")
                                      .c_str(),
                                  candidate.id == a.activeSpline)) {
                a.activeSpline = candidate.id;
                a.splineNode = -1;
                a.splineAdding = false;
            }
        ImGui::EndCombo();
    }
    s = a.world.findSpline(a.activeSpline);
    if (!s) {
        ImGui::TextWrapped(desktopTool() == 7 ? "Choose a thing, then click the map to start."
                                              : "Click an existing spline to select it.");
        ImGui::Text("New type: %d", a.thingType);
        return;
    }
    auto next = *s;
    bool changed = false;
    ImGui::SetNextItemWidth(-1);
    ImGui::TextUnformatted("Thing type");
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::InputInt("##splineType", &next.type, 0);
    if (ImGui::Button("Use palette thing", {-1, 0})) {
        next.type = a.thingType;
        next.angleOffset = 0;
        next.spacing = meshSpacing(next.type, next.angleOffset);
        changed = true;
    }
    ImGui::TextUnformatted("Spacing (map units)");
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::InputFloat("##splineSpacing", &next.spacing, 0, 0, "%.2f");
    if (ImGui::Button("Use mesh width", {-1, 0})) {
        next.angleOffset = 0;
        next.spacing = meshSpacing(next.type, next.angleOffset);
        changed = true;
    }
    ImGui::TextUnformatted("Yaw offset (degrees)");
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::InputFloat("##splineYaw", &next.angleOffset, 0, 0, "%.1f");
    changed |= ImGui::Checkbox("Smooth curve", &next.smooth);
    changed |= ImGui::Checkbox("Closed loop", &next.closed);
    if (changed)
        a.action([&] { changeSpline(a, next); });
    s = a.world.findSpline(a.activeSpline);
    if (!s)
        return;
    size_t copies = 0;
    for (const auto &t : a.world.things)
        if (t.splineId == s->id && t.selected >= 0)
            ++copies;
    ImGui::Text("%zu nodes, %zu copies", s->nodes.size(), copies);
    ImGui::TextUnformatted(a.splineExtendStart ? "Extend from: first point"
                                               : "Extend from: last point");
    if (ImGui::Button(a.splineAdding ? "Finish adding points" : "Add spline points", {-1, 0}))
        a.splineAdding = !a.splineAdding;
    if (a.splineNode >= 0 && a.splineNode < int(s->nodes.size())) {
        ImGui::Text("Point %d", a.splineNode + 1);
        auto updated = *s;
        float coords[]{s->nodes[a.splineNode].x, s->nodes[a.splineNode].y};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputFloat2("##splineNode", coords, "%.1f"))
            a.action([&] {
                updated.nodes[a.splineNode] = {coords[0], coords[1]};
                changeSpline(a, updated);
            });
        if (ImGui::Button("Remove point", {-1, 0}))
            removeNode(a);
    }
    if (ImGui::Button("New spline", {-1, 0}))
        setDesktopTool(a, 7);
    if (ImGui::Button("Bake to things", {-1, 0})) {
        a.world.checkpoint();
        a.world.bakeSpline(a.activeSpline);
        a.activeSpline = 0;
        a.splineNode = -1;
        a.message("Spline converted to independent things; undo restores its nodes");
    }
    if (ImGui::Button("Delete spline", {-1, 0})) {
        a.world.checkpoint();
        a.world.deleteSpline(a.activeSpline);
        a.activeSpline = 0;
        a.splineNode = -1;
    }
    ImGui::TextWrapped("Click first/last point, then click the map to extend that end. Drag: move "
                       "point. Shift-click curve: insert. Ctrl-click point or Delete: remove. "
                       "Right-click / Enter: finish.");
}
void respondThingSpline(App &a, bool hovered) {
    auto &io = ImGui::GetIO();
    auto &w = a.world;
    if (ImGui::IsMousePosValid())
        a.view.mouse = screenToWorld(a, io.MousePos);
    bool keyboard = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                    !io.WantTextInput && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    if (keyboard) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
            a.saveMap();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            w.undo();
            a.splineDragging = false;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
            w.redo();
            a.splineDragging = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Home))
            fitMap(a);
        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            removeNode(a);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            a.splineAdding = a.splineDragging = false;
        }
    }
    if (hovered)
        panZoom(a);
    if (hovered && ImGui::IsMouseClicked(1)) {
        a.splineAdding = false;
        a.splineDragging = false;
    }
    if (hovered && ImGui::IsMouseClicked(0))
        a.action([&] {
            float tolerance = 10 * a.dpi / a.view.zoom, best = tolerance;
            int hitSpline = 0, hitNode = -1;
            for (const auto &s : w.thingSplines)
                for (size_t i = 0; i < s.nodes.size(); ++i) {
                    float d =
                        std::hypot(s.nodes[i].x - a.view.mouse.x, s.nodes[i].y - a.view.mouse.y);
                    if (d < best) {
                        best = d;
                        hitSpline = s.id;
                        hitNode = int(i);
                    }
                }
            if (hitNode >= 0) {
                a.activeSpline = hitSpline;
                a.splineNode = hitNode;
                if (io.KeyCtrl) {
                    removeNode(a);
                    return;
                }
                const auto *s = w.findSpline(hitSpline);
                const bool endpoint = hitNode == 0 || hitNode == int(s->nodes.size()) - 1;
                a.splineAdding = endpoint;
                if (endpoint)
                    a.splineExtendStart = hitNode == 0 && s->nodes.size() > 1;
                a.splineDragging = true;
                a.view.dragging = false; // Checkpoint only on movement.
                a.view.lastDrag = snapped(a, a.view.mouse);
                return;
            }
            best = tolerance * 1.6f;
            int segment = -1;
            for (const auto &s : w.thingSplines) {
                auto samples = sampleThingSpline(s);
                for (size_t i = 1; i < samples.size(); ++i) {
                    float d = distanceToLine(a.view.mouse, samples[i - 1].point, samples[i].point);
                    if (d < best) {
                        best = d;
                        hitSpline = s.id;
                        segment = samples[i].segment;
                    }
                }
            }
            if (segment >= 0 && (io.KeyShift || !a.splineAdding || hitSpline != a.activeSpline)) {
                a.activeSpline = hitSpline;
                a.splineAdding = false;
                if (io.KeyShift) {
                    auto next = *w.findSpline(hitSpline);
                    int at = std::min(segment + 1, int(next.nodes.size()));
                    next.nodes.insert(next.nodes.begin() + at, snapped(a, a.view.mouse));
                    changeSpline(a, next);
                    a.splineNode = at;
                } else
                    a.splineNode = -1;
                return;
            }
            auto p = snapped(a, a.view.mouse);
            auto *s = w.findSpline(a.activeSpline);
            if (!s && desktopTool() == 7) {
                ThingSpline next;
                next.id = w.nextSplineId();
                next.type = a.thingType;
                next.spacing = meshSpacing(next.type, next.angleOffset);
                next.nodes.push_back(p);
                auto generated = generateSplineThings(next);
                w.checkpoint();
                w.deselect();
                w.thingSplines.push_back(next);
                w.rebuildSpline(next.id);
                a.activeSpline = next.id;
                a.splineNode = 0;
                a.splineAdding = true;
                a.splineExtendStart = false;
            } else if (s && a.splineAdding) {
                auto next = *s;
                if (a.splineExtendStart)
                    next.nodes.insert(next.nodes.begin(), p);
                else
                    next.nodes.push_back(p);
                changeSpline(a, next);
                a.splineNode = a.splineExtendStart ? 0 : int(next.nodes.size()) - 1;
            }
        });
    if (a.splineDragging) {
        auto *s = w.findSpline(a.activeSpline);
        if (!s || a.splineNode < 0 || a.splineNode >= int(s->nodes.size()) ||
            !ImGui::IsMouseDown(0)) {
            a.splineDragging = false;
            return;
        }
        auto p = snapped(a, a.view.mouse);
        if (!(p == a.view.lastDrag))
            a.action([&] {
                auto next = *s;
                next.nodes[a.splineNode] = p;
                auto generated = generateSplineThings(next);
                if (!a.view.dragging) {
                    w.checkpoint();
                    a.view.dragging = true;
                }
                *s = std::move(next);
                w.rebuildSpline(a.activeSpline);
                a.view.lastDrag = p;
            });
    } else
        a.view.dragging = false;
}
void drawThingSplines(App &a, ImDrawList *d) {
    bool editing = desktopTool() == 7 || desktopTool() == 8;
    for (const auto &s : a.world.thingSplines) {
        bool active = editing && s.id == a.activeSpline;
        auto color = active ? IM_COL32(80, 255, 170, 255) : IM_COL32(40, 160, 150, 180);
        auto samples = sampleThingSpline(s);
        for (size_t i = 1; i < samples.size(); ++i)
            d->AddLine(worldToScreen(a, samples[i - 1].point), worldToScreen(a, samples[i].point),
                       color, active ? 2 * a.dpi : a.dpi);
        if (editing)
            for (size_t i = 0; i < s.nodes.size(); ++i) {
                auto p = worldToScreen(a, s.nodes[i]);
                float r = 5 * a.dpi;
                d->AddCircleFilled(
                    p, r, active && int(i) == a.splineNode ? IM_COL32(255, 160, 30, 255) : color);
                d->AddCircle(p, r, IM_COL32(0, 0, 0, 255), 0, a.dpi);
                d->AddText({p.x + r + 3 * a.dpi, p.y - r}, color, std::to_string(i + 1).c_str());
            }
    }
}
} // namespace de
