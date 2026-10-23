#include "EditWorld.h"
#include "ThingSpline.h"
#include "Wadfile.h"
#include "idfunctions.h"
#include <cmath>
#include <iostream>
using namespace de;
int main(int argc, char **argv) {
    try {
        require(argc == 3, "Expected project root and output directory");
        std::filesystem::path root = argv[1], out = argv[2];
        std::filesystem::create_directories(out);
        auto close = [](float a, float b) { return std::abs(a - b) < .02f; };
        ThingSpline s;
        s.id = 1;
        s.type = 15002;
        s.spacing = 128;
        s.nodes = {{0, 0}, {512, 0}};
        auto baseline = generateSplineThings(s);
        require(baseline.size() == 4, "Long segment must repeat full mesh pitch");
        for (size_t i = 0; i < baseline.size(); ++i)
            require(close(baseline[i].origin.x, 64 + 128 * float(i)) && baseline[i].angle == 0,
                    "Incorrect panel centers/yaw");
        s.nodes = {{0, 0}, {1, 0}, {2, 0}, {7, 0}, {511, 0}, {512, 0}};
        auto dense = generateSplineThings(s);
        require(dense.size() == baseline.size(), "Node density changed copy count");
        for (size_t i = 0; i < dense.size(); ++i)
            require(close(dense[i].origin.x, baseline[i].origin.x),
                    "Uneven nodes changed distance-based placement");
        s.nodes = {{0, 0}, {0, 0}, {512, 0}, {512, 0}};
        require(generateSplineThings(s).size() == 4, "Coincident nodes must not divide by zero");
        s.nodes = {{0, 0}, {0, 512}};
        s.angleOffset = 15;
        for (const auto &t : generateSplineThings(s))
            require(t.angle == 105, "Follow curve yaw/offset failed");
        s.angleOffset = 0;
        s.spacing = 32;
        s.nodes = {{0, 0}, {12, 30}, {320, 60}, {400, 220}};
        auto curve = sampleThingSpline(s);
        auto things = generateSplineThings(s);
        double first = (curve.back().distance - (things.size() - 1) * s.spacing) / 2;
        for (size_t i = 0; i < things.size(); ++i) {
    
        auto expected = splinePointAt(curve, first + i * s.spacing);
            require(close(things[i].origin.x, expected.x) && close(things[i].origin.y, expected.y),
                    "Curve arc spacing failed");
        }
        s.smooth = false;
        s.nodes = {{0, 0}, {256, 0}, {256, 256}, {0, 256}};
        s.closed = true;
        s.spacing = 64;
        things = generateSplineThings(s);
        require(things.size() == 16, "Closed loop copy count");
        require(!(things.front().origin == things.back().origin),
                "Closed seam duplicated endpoint");
        s.nodes = {{0, 0}, {10, 0}};
        require(generateSplineThings(s).size() == 1, "Short spline must retain an initial thing");
        s.nodes = {{0, 0}};
        require(generateSplineThings(s).size() == 1, "Single point must place a thing");
        s.spacing = 0;
        bool rejected = false;
        try {
            generateSplineThings(s);
        } catch (...) {
            rejected = true;
        }
        require(rejected, "Zero spacing must be rejected");
        s.spacing = 64;
        s.closed = false;

        EditWorld world;
        world.load(root / "maps/IRONFENC.dwd");
        s.nodes = {{-256, -160}, {-248, -160}, {0, -160}, {256, -120}};
        s.smooth = true;
        world.thingSplines.push_back(s);
        world.rebuildSpline(s.id);
        const auto count = world.things.size();
        world.rebuildSpline(s.id);
        require(world.things.size() == count, "Regeneration accumulated duplicate things");
        world.markSaved();
        world.checkpoint();
        world.findSpline(1)->nodes[1].y = -100;
        world.rebuildSpline(1);
        require(world.dirty() && world.undo() && !world.dirty(), "Spline undo failed");
        require(world.redo() && world.dirty(), "Spline redo failed");
        world.reference = {
            (root / "assets/models/security-fence/iron.png").lexically_normal().u8string(),
            {12.25f, -4.5f},
            2.125f
,
            .37f,
            17.5f};
        world.save(out / "SPLINE.dwd");
        EditWorld reload;
        reload.load(out / "SPLINE.dwd");
        require(reload.thingSplines.size() == 1 && reload.thingSplines[0].nodes.size() == 4,
                "Spline nodes lost in DWD");
        require(reload.reference == world.reference && !reload.dirty(),
                "Reference transform/path roundtrip failed");
        require(reload.serialize() == world.serialize(),
                "DWD roundtrip changed spline ownership or positions");
        auto before = reload.things.size();
        reload.rebuildSpline(1);
        require(reload.things.size() == before, "Reloaded owner IDs must replace copies");
        std::filesystem::create_directories(out / "subfolder");
        reload.save(out / "subfolder" / "SPLINE.dwd");
        EditWorld moved;
        moved.load(out / "subfolder" / "SPLINE.dwd");
        require(moved.reference == world.reference, "Save As must rebase relative reference path");
        reload.checkpoint();
        reload.bakeSpline(1);
        require(reload.thingSplines.empty() && reload.things.size() == before, "Bake lost things");
        require(reload.undo() && reload.thingSplines.size() == 1, "Undo bake failed");
        reload.checkpoint();
        reload.deleteSpline(1);
        require(reload.thingSplines.empty() && reload.things.size() < before,
                "Delete spline retained children");
        require(reload.undo(), "Undo spline deletion failed");
        for (auto &t : reload.things)
            if (t.splineId == 1) {
                t.selected = 1;
                break;
            }
        auto node = reload.findSpline(1)->nodes[0];
        reload.checkpoint();
        reload.moveSelected({16, 24});
        require(reload.findSpline(1)->nodes[0] == node + Vec2{16, 24},
                "Moving a generated thing must carry its spline");
        require(reload.undo(), "Undo group move failed");
        reload.exportMap(out
 / "SPLINE-editable.wad", "SPLINE");
        Wadfile wad;
        wad.load(out / "SPLINE-editable.wad");
        require(wad.lumps[wad.find("THINGS")].data.size() == reload.things.size() * 10,
                "WAD lost generated ordinary things");
        const auto stable = reload.serialize();
        rejected = false;
        try {
            reload.parse(stable + "\n// DoomEd reference image v1\n\"image.png\"\n0 0 nan 1 0\n");
        } catch (...) {
            rejected = true;
        }
        require(rejected && stable == reload.serialize(),
                "Bad extension must fail transactionally");
        std::cout
            << "Thing spline regression passed: distance spacing, uneven/coincident nodes, yaw, "
               "curves, loops, undo/redo, ownership, DWD, reference paths, Save As, WAD export\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
