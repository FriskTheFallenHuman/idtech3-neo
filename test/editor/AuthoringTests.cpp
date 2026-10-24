// SPDX-License-Identifier: GPL-3.0-or-later
#include "SectorAuthoring.h"
#include "DoomProject.h"
#include "idfunctions.h"
#include "pathops.h"
#include <algorithm>
#include <iostream>
#include <limits>

using namespace de;
namespace {
void check(bool condition, const char *message) {
    require(condition, message);
}
template <typename Function> void rejectsUnchanged(EditWorld &world, Function function) {
    const auto before = world.serialize();
    bool rejected = false;
    try {
        function();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "Invalid authoring input was accepted");
    check(world.serialize() == before, "Invalid authoring input partially changed the map");
}
void checkRoom(bool reverse) {
    EditWorld world;
    world.clear();
    const auto empty = world.serialize();
    std::vector<Vec2> polygon = {{0, 0}, {128, 0}, {128, 128}, {0, 128}};
    if (reverse) {
        std::reverse(polygon.begin(), polygon.end());
    }
    authorSector(world, polygon);
    check(world.lines.size() == 4, "Room needs four walls");
    // Independent geometric assertion: the center lies to the right of every wall.
    for (const auto &line : world.lines) {
        const auto a = world.points[line.p1].pt, b = world.points[line.p2].pt;
        check(cross(b - a, Vec2{64, 64} - a) < 0, "Wall faces outside the room");
        check(!(line.flags & ML_TWOSIDED), "Isolated room unexpectedly has portals");
    }
    check(sectorAt(world, {64, 64}).size() == 4, "Room selection misses center");
    check(sectorAt(world, {-64, 64}).empty(), "Room selection includes outside");
    check(world.undo() && world.serialize() == empty, "Sector creation is not one undo step");
    check(world.redo(), "Sector creation redo failed");
    authorSector(world, {{128, 0}, {256, 0}, {256, 128}, {128, 128}});
    check(std::count_if(world.lines.begin(), world.lines.end(),
                        [](const Line &line) { return line.selected >= 0; }) == 7,
          "Adjacent room left duplicate shared walls");
    const auto portals =
        std::count_if(world.lines.begin(), world.lines.end(),
                      [](const Line &line) { return (line.flags & ML_TWOSIDED) != 0; });
    check(portals == 1, "Adjacent room needs exactly one shared portal");
    const auto left = sectorAt(world, {64, 64}), right = sectorAt(world, {192, 64});
    check(left.size() == 4 && right.size() == 4, "Adjacent sector selection failed");
    applySurfaceTexture(world, left, "STARTAN3", 3);
    applySurfaceTexture(world, left, "FLOOR0_1", 0);
    for (const auto [index, side] : left) {
        check(world.lines[index].side[side].ends.floorflat == "STARTAN3",
              "Wall texture could not be assigned to a floor");
        check(world.lines[index].side[side].midtexture == "FLOOR0_1",
              "Floor texture could not be assigned to a wall");
    }
    rejectsUnchanged(world, [&] { authorSector(world, {{0, 0}, {1, 1}}); });
    rejectsUnchanged(world, [&] { authorSector(world, {{0, 0}, {1, 1}, {2, 2}}); });
    rejectsUnchanged(world, [&] { authorSector(world, {{0, 0}, {128, 128}, {0, 128}, {128, 0}}); });
    rejectsUnchanged(world, [&] { authorSector(world, {{0, 0}, {0, 0}, {128, 0}, {128, 128}}); });
    rejectsUnchanged(world, [&] {
        authorSector(world, {{0, 0}, {std::numeric_limits<float>::infinity(), 0}, {0, 128}});
    });
    rejectsUnchanged(world, [&] { applySurfaceTexture(world, {{99999, 0}}, "STARTAN3", 0); });
    rejectsUnchanged(world, [&] { applySurfaceTexture(world, left, "../../bad", 0); });
    EditWorld restored;
    restored.parse(world.serialize());
    check(restored.serialize() == world.serialize(), "DWD authoring round trip changed data");
}
} // namespace

int main(int argc, char **argv) {
    try {
        checkRoom(false);
        checkRoom(true);
        EditWorld objects;
        objects.clear();
        const auto emptyObjects = objects.serialize();
        objects.placeObject({64, 96}, 3001, -90, 15);
        check(objects.things.size() == 1 && objects.things[0].type == 3001 &&
                  objects.things[0].angle == 270 && objects.things[0].options == 15,
              "Object placement lost the selected type or defaults");
        objects.placeObject({128, 96}, 2001);
        check(objects.things[0].selected == 0 && objects.things[1].type == 2001 &&
                  objects.things[1].selected == 1,
              "Changing the placement type modified an existing object");
        EditWorld objectCopy;
        objectCopy.parse(objects.serialize());
        check(objectCopy.things[0].type == 3001 && objectCopy.things[1].type == 2001,
              "DWD did not retain the chosen object types");
        rejectsUnchanged(objects, [&] { objects.placeObject({0, 0}, -1); });
        check(objects.undo() && objects.things.size() == 1 && objects.undo() &&
                  objects.serialize() == emptyObjects,
              "Object placement undo was not atomic");
        EditWorld divided;
        divided.clear();
        authorSector(divided, {{0, 0}, {512, 0}, {512, 512}, {0, 512}});
        const auto original = divided.serialize();
        authorDivider(divided, {{0, 256}, {512, 256}});
        check(connectSectors(divided).size() == 2, "Divider failed to split a sector");
        check(divided.undo() && divided.serialize() == original, "Divider undo was not atomic");
        authorSector(divided, {{128, 128}, {256, 128}, {256, 256}, {128, 256}});
        check(connectSectors(divided).size() == 2, "Inset sector creation failed");
        check(sectorAt(divided, {192, 192}).size() == 4, "Inset sector selection failed");
        if (argc == 2) {
            DoomProject project;
            project.loadWad(argv[1]);
            const auto floor = std::find_if(project.textures.begin(), project.textures.end(),
                                            [](const Texture &t) { return t.name == "FLOOR0_1"; });
            const auto wall = std::find_if(project.textures.begin(), project.textures.end(),
                                           [](const Texture &t) { return t.name == "STARTAN3"; });
            check(floor != project.textures.end() && wall != project.textures.end(),
                  "Unified texture catalog omits a storage kind");
            check(project.wad.surfaceImage(floor->name).width == 64, "Floor image decode failed");
            check(project.wad.surfaceImage(wall->name).width > 0, "Wall image decode failed");
        }
        std::cout << "Sector authoring: both windings, atomic undo, portals, picking, unified "
                     "textures, DWD round trip and malformed input passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
