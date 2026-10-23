#pragma once
#include "../UI.h"
#include "../ThingSpline.h"
#include "../DesktopPanels.h"
#include "../idfunctions.h"
#include <iostream>
namespace de {
// Runs inside a real ImGui/D3D map frame against the production mouse handler.
inline void testThingSplineEditing(App &a, const std::filesystem::path &out) {
    auto &io = ImGui::GetIO();
    auto mouse = io.MousePos;
    bool clicked = io.MouseClicked[0], down = io.MouseDown[0], shift = io.KeyShift,
         ctrl = io.KeyCtrl;
    float duration = io.MouseDownDuration[0];
    a.prefs.snap = true;
    a.prefs.grid = 8;
    a.thingType = 15003;
    setDesktopTool(a, 7);
    auto click = [&](Vec2 p, bool control = false, bool insert = false) {
        io.MousePos = worldToScreen(a, p);
        io.MouseClicked[0] = true;
        io.MouseDown[0] = true;
        io.MouseDownDuration[0] = 0;
        io.KeyCtrl = control;
        io.KeyShift = insert;
        respondThingSpline(a, true);
        io.MouseClicked[0] = false;
        io.MouseDown[0] = false;
        io.KeyCtrl = io.KeyShift = false;
        io.MouseDownDuration[0] = -1;
        respondThingSpline(a, true);
        require(a.error.empty(), a.error);
    };
    click({-256, -128});
    int id = a.activeSpline;
    require(a.world.findSpline(id) && a.world.findSpline(id)->spacing == 64,
            "Spline creation must use selected model width");
    click({-240, -128});
    click({0, -96});
    click({256, -128});
    require(a.world.findSpline(id)->nodes.size() == 4, "Create tool did not append nodes");
    auto copies = a.world.things.size();
    require(copies > 13, "Far nodes failed to create repeated meshes");
    setDesktopTool(a, 8);
    click({0, -96});
    require(a.activeSpline == id && a.splineNode == 2, "Edit tool failed to select existing node");
    // Drag the selected node through several updates, with one undo step.
    io.MousePos = worldToScreen(a, {0, -96});
    io.MouseClicked[0] = io.MouseDown[0] = true;
    io.MouseDownDuration[0] = 0;
    respondThingSpline(a, true);
    io.MouseClicked[0] = false;
    io.MouseDownDuration[0] = .1f;
    io.MousePos = worldToScreen(a, {0, -80});
    respondThingSpline(a, true);
    io.MousePos = worldToScreen(a, {0, -64});
    respondThingSpline(a, true);
    io.MouseDown[0] = false;
    respondThingSpline(a, true);
    require(a.world.findSpline(id)->nodes[2] == Vec2{0, -64}, "Node drag failed");
    require(a.world.undo() && a.world.findSpline(id)->nodes[2] == Vec2{0, -96},
            "Continuous drag was not one undo step");
    require(a.world.redo() && a.world.findSpline(id)->nodes[2] == Vec2{0, -64}, "Drag redo failed");
    auto samples = sampleThingSpline(*a.world.findSpline(id));
    auto insertion = splinePointAt(samples, samples.back().distance * .7);
    click(insertion, false, true);
    require(a.world.findSpline(id)->nodes.size() == 5, "Shift-click did not insert a point");
    auto inserted = a.world.findSpline(id)->nodes[a.splineNode];
    click(inserted, true);
    require(a.world.findSpline(id)->nodes.size() == 4, "Ctrl-click did not remove a point");
    // Appending works in Edit mode too.
    a.splineAdding = true;
    click({288, -64});
    require(a.world.findSpline(id)->nodes.size() == 5, "Edit tool failed to append nodes");
    setDesktopTool(a, 8);
    click(a.world.findSpline(id)->nodes.front());
    require(a.splineAdding && a.splineExtendStart,
            "First endpoint did not activate start extension");
    click({-288, -144});
    click({-320, -160});
    require(a.world.findSpline(id)->nodes.size() == 7 &&
                a.world.findSpline(id)->nodes.front() == Vec2{-320, -160},
            "Start extension failed to prepend consecutive nodes");
    require(a.world.undo() && a.world.findSpline(id)->nodes.front() == Vec2{-288, -144},
            "Start extension undo failed");
    require(a.world.redo(), "Start extension redo failed");
    click(a.world.findSpline(id)->nodes.back());
    require(a.splineAdding && !a.splineExtendStart, "Last endpoint did not activate end extension");
    click({320, -32});
    require(a.world.findSpline(id)->nodes.size() == 8 &&
                a.world.findSpline(id)->nodes.back() == Vec2{320, -32},
            "End extension failed to append");
    a.splineAdding = false;
    std::filesystem::create_directories(out.parent_path());
    a.world.save(out);
    EditWorld restored;
    restored.load(out);
    require(restored.findSpline(id) && restored.findSpline(id)->nodes.size() == 8,
            "UI-created spline did not survive DWD load");
    a.world = std::move(restored);
    a.activeSpline = id;
    a.splineNode = 2;
    io.MousePos = mouse;
    io.MouseClicked[0] = clicked;
    io.MouseDown[0] = down;
    io.KeyShift = shift;
    io.KeyCtrl = ctrl;
    io.MouseDownDuration[0] = duration;
    std::cout << "Thing spline UI passed: create, automatic mesh width, append, select, drag, "
                 "one-step undo/redo, insert, remove, extension from either end, reload\n";
}
} // namespace de
