#include "MapViewResp.h"
#include "BlockWorld.h"
#include "pathops.h"
#include "NextUI.h"
#include <algorithm>
#include <cmath>
namespace de {
void placeThingAt(App &a, Vec2 p) {
    a.pickedSectorSides.clear();
    a.world.placeObject(snapped(a, p), a.thingType, a.thingAngle, a.thingOptions);
    a.view.mode = 2;
    showNativePanel("thing:Thing Inspector");
}
void mapRespond(App &a) {
    auto &v = a.view;
    auto &w = a.world;
    auto &io = ImGui::GetIO();
    Line drawingPrototype;
    for (const auto &line : w.lines)
        if (line.selected > 0) {
            drawingPrototype.side[0].ends =
                line.side[(line.flags & 4) ? line.selected - 1 : 0].ends;
            break;
        }
    auto joinClosedRooms = [&](bool checkpoint) {
        try {
            auto joined = w;
            if (joinAdjacentSectors(joined)) {
                if (checkpoint)
                    w.checkpoint();
                static_cast<WorldState &>(w) = static_cast<const WorldState &>(joined);
                a.pickedSectorSides.clear();
                a.message("Joined shared room boundaries");
            }
        } catch (const std::exception &) {
            // Open drawings and nested Alt+S loops remain editable without partial repairs.
        }
    };
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsMousePosValid())
        v.mouse = screenToWorld(a, io.MousePos);
    bool keyboard = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                    !io.WantTextInput && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
    if (hovered && ImGui::IsMouseClicked(0))
        a.pickedSectorSides.clear();
    if (keyboard) {
        const bool selectedThing = std::any_of(w.things.begin(), w.things.end(),
                                               [](const Thing &t) { return t.selected > 0; });
        if (selectedThing && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper && !v.drawing &&
            !v.dragging && !v.boxing) {
            const int step = io.KeyShift ? 1 : 8;
            const int delta = (ImGui::IsKeyPressed(ImGuiKey_1) ? step : 0) -
                              (ImGui::IsKeyPressed(ImGuiKey_2) ? step : 0);
            if (delta) {
                if (w.moveSelectedThingsVertically(delta))
                    a.message(delta > 0 ? "Raised selected things" : "Lowered selected things");
                return;
            }
        }
        if (hovered && !io.KeyCtrl && !io.KeyAlt && !io.KeyShift && !io.KeySuper && !v.drawing &&
            !v.dragging && !v.boxing && ImGui::IsKeyPressed(ImGuiKey_1, false)) {
            int line = -1;
            float best = 10 * a.dpi / v.zoom;
            for (size_t i = 0; i < w.lines.size(); ++i) {
                const auto &candidate = w.lines[i];
                if (candidate.selected < 0)
                    continue;
                const float distance =
                    distanceToLine(v.mouse, w.points[candidate.p1].pt, w.points[candidate.p2].pt);
                if (distance < best) {
                    best = distance;
                    line = int(i);
                }
            }
            if (line >= 0) {
                const auto &target = w.lines[line];
                const Vec2 start = w.points[target.p1].pt;
                const Vec2 delta = w.points[target.p2].pt - start;
                const float lengthSquared = delta.x * delta.x + delta.y * delta.y;
                if (lengthSquared > 0) {
                    const Vec2 offset = v.mouse - start;
                    const float t = std::clamp(
                        (offset.x * delta.x + offset.y * delta.y) / lengthSquared, 0.f, 1.f);
                    // Use the same grid snapping as drawing and dragging vertices.
                    const Vec2 insertion = snapped(a, start + delta * t);
                    const float endpointMargin = 1.f / std::sqrt(lengthSquared);
                    if (t > endpointMargin && t < 1.f - endpointMargin && !(insertion == start) &&
                        !(insertion == w.points[target.p2].pt)) {
                        w.splitLine(line, insertion);
                        a.pickedSectorSides.clear();
                        a.message("Inserted vertex on linedef");
                    }
                }
            }
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            v.drawing = v.dragging = v.boxing = false;
            w.deselect();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Home))
            fitMap(a);
        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            w.deleteSelected();
        if (io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_S))
                a.saveMap();
            if (ImGui::IsKeyPressed(ImGuiKey_O))
                a.openMap();
            if (ImGui::IsKeyPressed(ImGuiKey_N))
                a.newMap();
            if (ImGui::IsKeyPressed(ImGuiKey_Z))
                w.undo();
            if (ImGui::IsKeyPressed(ImGuiKey_Y))
                w.redo();
            if (ImGui::IsKeyPressed(ImGuiKey_A)) {
                if (v.mode == 3) {
                    w.selectAll(0);
                    w.selectAll(1);
                    w.selectAll(2);
                } else
                    w.selectAll(v.mode);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_C))
                w.copy();
            if (ImGui::IsKeyPressed(ImGuiKey_X)) {
                w.copy();
                w.deleteSelected();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_V))
                w.paste(snapped(a, hovered ? v.mouse : v.center));
        } else if (ImGui::IsKeyPressed(ImGuiKey_F))
            w.flipSelected();
    }
    if (hovered) {
        panZoom(a);
        if (ImGui::IsMouseClicked(1)) {
            if (v.drawing)
                joinClosedRooms(true);
            v.drawing = false;
        }
        if (ImGui::IsMouseClicked(0)) {
            auto p = snapped(a, v.mouse);
            if (v.tool == 3) {
                placeThingAt(a, p);
            } else if (v.tool == 1 || v.tool == 2) {
                if (!v.drawing) {
                    v.lineStart = v.lineFirst = p;
                    v.drawing = true;
                } else if (!(p == v.lineStart)) {
                    w.checkpoint();
                    if (v.tool == 1) {
                        const int lastLine = w.newLine(v.lineStart, p, drawingPrototype);
                        v.lineStart = p;
                        const bool closedLoop = p == v.lineFirst;
                        const bool formedSector = finishDrawnSector(w, lastLine, closedLoop);
                        if (closedLoop || formedSector)
                            v.drawing = false;
                        if (formedSector) {
                            a.pickedSectorSides.clear();
                            a.message("Created sector from connected boundaries");
                        }
                    } else {
                        Vec2 lo{std::min(p.x, v.lineStart.x), std::min(p.y, v.lineStart.y)},
                            hi{std::max(p.x, v.lineStart.x), std::max(p.y, v.lineStart.y)};
                        if (lo.x != hi.x && lo.y != hi.y) {
                            w.newLine({lo.x, hi.y}, hi, drawingPrototype);
                            w.newLine(hi, {hi.x, lo.y}, drawingPrototype);
                            w.newLine({hi.x, lo.y}, lo, drawingPrototype);
                            const int lastLine = w.newLine(lo, {lo.x, hi.y}, drawingPrototype);
                            if (finishDrawnSector(w, lastLine, true))
                                a.message("Created sector from closed rectangle");
                        }
                        v.drawing = false;
                    }
                    // Wait until the drawing ends so its final shared edge can be merged too.
                    if (!v.drawing)
                        joinClosedRooms(false);
                }
            } else {
                int hit = -1;
                float best = 10 * a.dpi / v.zoom;
                auto test = [&](float dist, int i) {
                    if (dist < best) {
                        best = dist;
                        hit = i;
                    }
                };
                if (v.mode == 0) {
                    for (size_t i = 0; i < w.points.size(); ++i)
                        if (w.points[i].selected >= 0)
                            test(distanceToLine(v.mouse, w.points[i].pt, w.points[i].pt), int(i));
                } else if (v.mode == 1) {
                    for (size_t i = 0; i < w.lines.size(); ++i)
                        if (w.lines[i].selected >= 0)
                            test(distanceToLine(v.mouse, w.points[w.lines[i].p1].pt,
                                                w.points[w.lines[i].p2].pt),
                                 int(i));
                } else if (v.mode == 2) {
                    for (size_t i = 0; i < w.things.size(); ++i)
                        if (w.things[i].selected >= 0)
                            test(distanceToLine(v.mouse, w.things[i].origin, w.things[i].origin),
                                 int(i));
                }
                if (v.mode == 0 && hit < 0 && ImGui::IsMouseDoubleClicked(0)) {
                    int line = -1;
                    float d = 10 * a.dpi / v.zoom;
                    for (size_t i = 0; i < w.lines.size(); ++i)
                        if (w.lines[i].selected >= 0) {
                            auto &l = w.lines[i];
                            float dist =
                                distanceToLine(v.mouse, w.points[l.p1].pt, w.points[l.p2].pt);
                            if (dist < d) {
                                d = dist;
                                line = int(i);
                            }
                        }
                    if (line >= 0) {
                        w.splitLine(line, p);
                        return;
                    }
                }
                if (hit >= 0) {
                    int *sel = v.mode == 0   ? &w.points[hit].selected
                               : v.mode == 1 ? &w.lines[hit].selected
                                             : &w.things[hit].selected;
                    if (io.KeyShift)
                        *sel = *sel > 0 ? 0 : 1;
                    else {
                        if (*sel == 0)
                            w.deselect();
                        *sel = 1;
                    }
                    if (*sel > 0) {
                        if (v.mode == 1 && io.KeyAlt && (w.lines[hit].flags & 4))
                            *sel = 2;
                        w.checkpoint();
                        v.dragging = true;
                        v.dragStart = v.lastDrag = p;
                    }
                } else {
                    if (!io.KeyShift)
                        w.deselect();
                    v.boxing = true;
                    v.boxStart = v.mouse;
                }
            }
        }
    }
    if (v.dragging) {
        if (ImGui::IsMouseDown(0)) {
            auto p = snapped(a, v.mouse);
            w.moveSelected(p - v.lastDrag);
            v.lastDrag = p;
        } else
            v.dragging = false;
    }
    if (v.boxing && ImGui::IsMouseReleased(0)) {
        Vec2 lo{std::min(v.boxStart.x, v.mouse.x), std::min(v.boxStart.y, v.mouse.y)},
            hi{std::max(v.boxStart.x, v.mouse.x), std::max(v.boxStart.y, v.mouse.y)};
        auto inside = [&](Vec2 p) {
            return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y;
        };
        if (v.mode == 0 || v.mode == 3) {
            for (auto &p : w.points)
                if (p.selected >= 0 && inside(p.pt))
                    p.selected = 1;
        }
        if (v.mode == 1 || v.mode == 3) {
            for (auto &l : w.lines)
                if (l.selected >= 0 && lineInRect(w.points[l.p1].pt, w.points[l.p2].pt, lo, hi))
                    l.selected = 1;
        }
        if (v.mode == 2 || v.mode == 3) {
            for (auto &t : w.things)
                if (t.selected >= 0 && inside(t.origin))
                    t.selected = 1;
        }
        v.boxing = false;
    }
}
} // namespace de
