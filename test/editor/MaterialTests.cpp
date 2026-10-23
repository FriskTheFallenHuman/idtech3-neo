#include "DoomProject.h"
#include "EditWorld.h"
#include "BlockWorld.h"
#include "idfunctions.h"
#include <algorithm>
#include <iostream>
using namespace de;
template <class F> void rejects(F fn) {
    bool failed = false;
    try {
        fn();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Expected invalid material pack to be rejected");
}
int main(int argc, char **argv) {
    try {
        require(argc == 3, "Expected Doom directory and output directory");
        {
            const auto emissive = doom_materials::parse(
                "DOOM_MATERIALS 1\nW TESTGLOW 64 64 EMISSIVE 1 .55 .16 220 768\nF NORMAL 64 64\n");
            const auto roundtrip = doom_materials::parse(doom_materials::serialize(emissive));
            require(roundtrip[0].emission.strength == 220 && roundtrip[0].emission.range == 768 &&
                        roundtrip[1].emission.strength == 0,
                    "Emissive metadata must survive material pack export");
            rejects([] {
                doom_materials::parse("DOOM_MATERIALS 1\nW BAD 64 64 EMISSIVE 2 1 1 10 512\n");
            });
            rejects([] {
                doom_materials::parse("DOOM_MATERIALS 1\nW BAD 64 64 EMISSIVE 1 1 1 -1 512\n");
            });
            rejects([] { doom_materials::parse("DOOM_MATERIALS 1\nW BAD 64 64 EMISSIVE 1 1\n"); });
            const auto broken = doom_materials::parse(
                "DOOM_MATERIALS 1\nF BROKEN 128 128 EMISSIVE 1 .85 .6 2.5 512 FLICKER\n");
            require(doom_materials::parse(doom_materials::serialize(broken))[0].emission.flicker,
                    "Broken-light animation must survive material export");
            rejects([] {
                doom_materials::parse(
                    "DOOM_MATERIALS 1\nF BAD 64 64 EMISSIVE 1 1 1 2 512 UNKNOWN\n");
            });
            rejects([] {
                doom_materials::parse(
                    "DOOM_MATERIALS 1\nF BAD 64 64 EMISSIVE 1 1 1 2 512 
FLICKER EXTRA\n");
            });
            const auto halo = doom_materials::parse(
                "DOOM_MATERIALS 1\nF LAMP 64 64 EMISSIVE 1 .55 .16 220 768 CORONA 24 .7\n");
            const auto haloCopy =
                doom_materials::parse(doom_materials::serialize(halo))[0].emission;
            require(haloCopy.coronaRadius == 24 && haloCopy.coronaStrength == .7f,
                    "Corona settings must survive material export");
            for (const auto *option :
                 {"CORONA 0 .7", "CORONA 24 -1", "CORONA 24", "CORONA 24 .7 CORONA 12 .5"})
                rejects([&] {
                    doom_materials::parse(
                        std::string("DOOM_MATERIALS 1\nF BAD 64 64 EMISSIVE 1 1 1 2 512 ") +
                        option + "\n");
                });
            bool bright = false, dark = false;
            for (unsigned tick = 0; tick < 350; ++tick) {
                const float value = doom_materials::emissionBrightness(broken[0].emission, tick);
                require(value >= 0 && value <= 1, "Flicker brightness must remain bounded");
                require(doom_materials::emissionBrightness(emissive[0].emission, tick) == 1,
                        "Steady streetlights must not flicker");
                bright |= value == 1;
                dark |= value < .05f;
            }
            require(bright && dark, "Broken light needs both lit and dropout intervals");
        }
        for (bool reverse : {false, true}) {
            EditWorld automatic;
            const Vec2 outer[] = {{0, 256}, {256, 256}, {256, 0}, {0, 0}};
            for (int i = 0; i < 4; ++i)
                automatic.newLine(outer[i], outer[(i + 1) % 4]);
            const auto before = automatic.serialize();
            automatic.checkpoint();
            int divider = automatic.newLine(reverse ? Vec2{128, 256} : Vec2{128, 0},
                                            reverse ? Vec2{128, 0} : Vec2{128, 256});
            require(finishDra
wnSector(automatic, divider, false),
                    "Reconnecting line automatically forms sector");
            require(connectSectors(automatic).size() == 2, "Automatic divider yields two sectors");
            require(automatic.undo() && automatic.serialize() == before,
                    "Automatic connection undo");
            std::vector<Vec2> inner{{64, 192}, {192, 192}, {192, 64}, {64, 64}};
            if (reverse)
                std::reverse(inner.begin(), inner.end());
            int last = -1;
            for (int i = 0; i < 4; ++i) {
                last = automatic.newLine(inner[i], inner[(i + 1) % 4]);
                const auto draft = automatic.serialize();
                const bool formed = finishDrawnSector(automatic, last, i == 3);
                require(formed == (i == 3), "Inner loop becomes a sector only when it closes");
                if (!formed)
                    require(automatic.serialize() == draft, "Open drawing remains unchanged");
            }
            require(connectSectors(automatic).size() == 2, "Automatic inner sector topology");
        }
        {
            EditWorld connected;
            const Vec2 outer[] = {{0, 128}, {128, 128}, {128, 0}, {0, 0}};
            for (int i = 0; i < 4; ++i)
                connected.newLine(outer[i], outer[(i + 1) % 4]);
            const Vec2 path[] = {{128, 128}, {256, 128}, {256, 0}, {128, 0}};
            for (int i = 0; i < 3; ++i) {
                int line = connected.newLine(path[i], path[i + 1]);
                require(finishDrawnSector(connected, line, false) == (i == 2),
                        "Three-edge room completes when it reconnects to existing boundary");
            }
            require(connectSectors(connected).size() == 2,
                    "Attached room closes without retracing shared wall");
        }
        {
            EditWorld attached;
            attached.load(std::filesystem::path(argv[1]) /
                          "tools/DoomEd/tests/
fixtures/AttachedInner.dwd");
            const auto original = attached;
            require(finishDrawnSector(attached, 10, false),
                    "Auto-connect three edges to an existing inner sector");
            require(connectSectors(attached).size() == 3,
                    "Parent plus two adjoining inner sectors");
            for (int i = 0; i < 8; ++i)
                for (int side = 0; side < ((original.lines[i].flags & 4) ? 2 : 1); ++side)
                    require(attached.lines[i].side[side].ends == original.lines[i].side[side].ends,
                            "Existing parent and sloped inner sector properties remain unchanged");
            for (int i = 8; i < 11; ++i)
                require((attached.lines[i].flags & 5) == 4 &&
                            attached.lines[i].side[0].midtexture == "-" &&
                            attached.lines[i].side[1].midtexture == "-",
                        "New inner edges have open portals on both sides");
            require(attached.validate().empty(), "Attached sloped-sector fixture validates");
            std::filesystem::create_directories(argv[2]);
            attached.save(std::filesystem::path(argv[2]) / "ATTACHED.dwd");
        }
        // Drawing adjacent rectangles must produce one two-sided shared boundary.
        for (bool partial : {false, true})
            for (bool explicitIds : {false, true}) {
                EditWorld joined;
                auto box = [&](float left, float bottom, float right, float top, int floor,
                               int id) {
                    const Vec2 p[] = {{left, top}, {right, top}, {right, bottom}, {left, bottom}};
                    for (int i = 0; i < 4; ++i) {
                        Line line;
                        line.side[0].ends.floorheight = floor;
                        line.side[0].sector = id;
                        joined.newLine(p[i], p[(i + 1) % 4], line);
                    }
                };
                box
(0, 0, 128, 256, 0, explicitIds ? 17 : -1);
                box(128, partial ? 64 : 0, 256, partial ? 192 : 256, 16, -1);
                joined.explicitSectors = explicitIds;
                const auto before = joined.serialize();
                joined.checkpoint();
                require(joinAdjacentSectors(joined) > 0, "Adjacent rectangle shared wall joining");
                require(connectSectors(joined).size() == 2,
                        "Joined rooms retain independent sectors");
                int portals = 0;
                for (const auto &line : joined.lines)
                    if (line.selected >= 0 && (line.flags & 4)) {
                        ++portals;
                        require(!(line.flags & 1) && line.side[0].midtexture == "-" &&
                                    line.side[1].midtexture == "-",
                                "Shared wall opens into neighboring room");
                        require(line.side[0].ends.floorheight != line.side[1].ends.floorheight,
                                "Joining preserves each room's floor height");
                    }
                require(portals == 1, "Shared edge is not duplicated");
                require(joinAdjacentSectors(joined) == 0, "Joining is idempotent");
                if (!partial && !explicitIds) {
                    joined.things.push_back({{64, 128}, 0, 1, 7});
                    std::filesystem::create_directories(argv[2]);
                    joined.save(std::filesystem::path(argv[2]) / "ADJACENT.dwd");
                }
                require(joined.undo() && joined.serialize() == before, "Shared wall repair undo");
                require(joined.redo(), "Shared wall repair redo");
            }
        {
            EditWorld divided;
            const Vec2 p[] = {{0, 128}, {256, 128}, {256, 0}, {0, 0}};
            for (int i = 0; i < 4; ++i)
                divided.newLine(p[i], p[(i + 1) % 4]);
            divided.newLine({128, 0}, {128, 128});
            
require(joinAdjacentSectors(divided) > 0 && connectSectors(divided).size() == 2,
                    "A one-sided divider becomes a portal between two bounded rooms");
            EditWorld open;
            open.newLine({0, 0}, {128, 0});
            const auto before = open.serialize();
            rejects([&] { joinAdjacentSectors(open); });
            require(open.serialize() == before,
                    "Failed joining leaves an incomplete drawing unchanged");
        }
        for (bool explicitIds : {false, true})
            for (bool reversed : {false, true}) {
                EditWorld nested;
                nested.clear();
                auto box = [&](float lo, float hi, bool reverse) {
                    std::vector<Vec2> p{{lo, hi}, {hi, hi}, {hi, lo}, {lo, lo}};
                    if (reverse)
                        std::reverse(p.begin(), p.end());
                    for (int i = 0; i < 4; ++i)
                        nested.newLine(p[i], p[(i + 1) % 4]);
                };
                box(0, 256, false);
                nested.explicitSectors = explicitIds;
                for (auto &l : nested.lines) {
                    l.side[0].sector = explicitIds ? 7 : -1;
                    l.side[0].ends.floorflat = "CWFLOOR";
                    l.side[0].midtexture = "CWBRICK";
                }
                box(64, 192, reversed);
                const auto before = nested.serialize();
                auto selected = makeInnerSector(nested, {128, 128});
                require(selected.size() == 4, "Inner loop selection");
                require(connectSectors(nested).size() == 2,
                        "Inner and surrounding sectors must remain distinct");
                for (auto [index, side] : selected) {
                    auto &l = nested.lines[index];
                    require((l.flags & 5) == 4 && l.side[0].midtexture == "-" &&
                                l.side[1].midtexture == "-",
                            "Inner b
oundary must be an open two-sided portal");
                    require(l.side[0].ends.floorflat == "CWFLOOR" &&
                                l.side[1].bottomtexture == "CWBRICK",
                            "Inner sector must inherit surrounding materials");
                    l.side[side].ends.floorheight += 8;
                }
                require(connectSectors(nested).size() == 2, "Raised inner floor must remain valid");
                if (!explicitIds && !reversed) {
                    std::filesystem::create_directories(argv[2]);
                    nested.things.push_back({{32, 128}, 0, 1, 7});
                    nested.save(std::filesystem::path(argv[2]) / "INNER.dwd");
                }
                require(nested.undo() && nested.serialize() == before,
                        "Inner sector creation undo");
                require(nested.redo(), "Inner sector creation redo");
                rejects([&] { makeInnerSector(nested, {400, 400}); });
            }
        const std::filesystem::path root = argv[1], output = argv[2];
        std::filesystem::create_directories(output);
        {
            EditWorld ramp;
            ramp.clear();
            auto edge = [&](Vec2 a, Vec2 b, bool upper) {
                Line line;
                line.flags |= 16; // Bottom pegging must not shear rows on an incline.
                line.side[0].midtexture = "STARTAN3";
                if (upper)
                    line.side[0].ends.slope = {0, 128, 0, -16384};
                ramp.newLine(a, b, line);
            };
            // Ramp y=128..-128: 0..64, adjoining flat room y=128..384 at z=0.
            edge({-128, 128}, {-128, 384}, false);
            edge({-128, 384}, {128, 384}, false);
            edge({128, 384}, {128, 128}, false);
            edge({128, 128}, {128, -128}, true);
            edge({128, -128}, {-128, -128}, true);
            edge({-128, -128}, {-128, 128}, true);
            Line portal;
            portal.flags = 4;
   
         portal.side[0].midtexture = portal.side[1].midtexture = "-";
            portal.side[0].toptexture = portal.side[1].toptexture = "STARTAN3";
            portal.side[0].bottomtexture = portal.side[1].bottomtexture = "STARTAN3";
            portal.side[0].ends.slope = {0, 128, 0, -16384};
            ramp.newLine({-128, 128}, {128, 128}, portal);
            ramp.things.push_back({{0, 256}, 270, 1, 7});
            require(ramp.validate().empty(), "Ramp geometry validation");
            auto sectors = connectSectors(ramp);
            require(sectors.size() == 2, "Different planes must remain distinct sectors");
            ramp.explicitSectors = true;
            const auto saved = ramp.serialize();
            EditWorld restoredRamp;
            restoredRamp.parse(saved);
            require(restoredRamp.serialize() == saved, "Slope and explicit sector DWD round trip");
            ramp.exportMap(output / "RAMP-edit.wad", "RAMP");
            Wadfile rampWad;
            rampWad.load(output / "RAMP-edit.wad");
            restoredRamp.importMap(rampWad, "RAMP");
            require(connectSectors(restoredRamp).size() == 2, "Slope WAD round trip");
            for (size_t i = 0; i < ramp.lines.size(); ++i)
                for (int side = 0; side < ((ramp.lines[i].flags & 4) ? 2 : 1); ++side)
                    require(restoredRamp.lines[i].side[side].ends == ramp.lines[i].side[side].ends,
                            "WAD sector plane correspondence");
            require(ramp.lines[3].side[0].ends.floorAt({0, -128}) == 64, "Ramp endpoint height");
            const auto flat = ramp.lines[0].side[0].ends;
            const auto tilted = ramp.adjustedFloor(flat, 0, 4096);
            require(tilted.slope.active() && tilted.floorAt({-128, 256}) == 0 &&
                        tilted.floorAt({0, 256}) == 8,
                    "Slope shortcut pivot and rise/run");
            require(ramp.adjustedFloor(flat, 8, 0).floorheight == 8, "Height shortcut step");
   
         require(ramp.adjustedFloor(flat, 512, 0) == flat, "Reject floor above ceiling");
            ramp.save(output / "RAMP.dwd");
            auto roof = ramp;
            for (auto &line : roof.lines)
                for (auto &side : line.side) {
                    side.ends.ceilingSlope = side.ends.slope;
                    side.ends.ceilingSlope.dx = -side.ends.ceilingSlope.dx;
                    side.ends.ceilingSlope.dy = -side.ends.ceilingSlope.dy;
                    side.ends.slope = {};
                }
            require(roof.validate().empty(), "Ceiling ramp validation");
            require(roof.lines[3].side[0].ends.ceilingAt({0, 0}) == 96, "Ceiling plane height");
            const auto roofText = roof.serialize();
            EditWorld restoredRoof;
            restoredRoof.parse(roofText);
            require(restoredRoof.serialize() == roofText, "Ceiling DWD round trip");
            roof.exportMap(output / "ROOF-edit.wad", "ROOF");
            Wadfile roofWad;
            roofWad.load(output / "ROOF-edit.wad");
            restoredRoof.importMap(roofWad, "ROOF");
            require(restoredRoof.lines[3].side[0].ends.ceilingAt({0, 0}) == 96,
                    "Ceiling-only WAD round trip");
            roof.save(output / "ROOF.dwd");
            const auto &roofSector = roof.lines[3].side[0].ends;
            const auto raisedRoof = roof.adjustedSurface(roofSector, true, 8, 0);
            require(raisedRoof.ceilingAt({0, 0}) == 104 && raisedRoof.floorheight == 0,
                    "Ceiling height shortcut leaves floor unchanged");
            const auto tiltedRoof = roof.adjustedSurface(flat, true, 0, 4096);
            require(tiltedRoof.ceilingSlope.active() && !tiltedRoof.slope.active(),
                    "Ceiling slope shortcut leaves floor unchanged");
            require(roof.adjustedSurface(roofSector, true, -128, 0) == roofSector,
                    "Reject ceiling below floor");
            roof.checkpoint();
            
for (auto &line : roof.lines)
                line.selected = 1;
            roof.moveSelected({64, 32});
            require(roof.lines[3].side[0].ends.ceilingAt({64, 32}) == 96,
                    "Moving complete sector preserves ceiling plane");
            require(roof.undo() && roof.serialize() == roofText, "Ceiling edit undo");
            require(roof.redo(), "Ceiling edit redo");

            auto moved = ramp;
            for (auto &line : moved.lines)
                line.selected = 1;
            moved.moveSelected({64, 32});
            require(moved.lines[3].side[0].ends.floorAt({64, -96}) == 64,
                    "Moving complete ramp preserves its relative heights");
            moved.copy();
            moved.clear();
            moved.paste({512, 512});
            require(!moved.lines.empty(), "Slope clipboard has geometry");
            require(moved.validate().empty(), "Pasted ramp retains valid slope planes");
            auto shortcut = ramp;
            const auto initial = shortcut.serialize();
            shortcut.checkpoint();
            const auto changed = shortcut.adjustedFloor(shortcut.lines[0].side[0].ends, 0, 4096,
                                                        {{0, 0}, {1, 0}, {2, 0}, {6, 1}});
            for (auto [line, side] :
                 std::vector<std::pair<int, int>>{{0, 0}, {1, 0}, {2, 0}, {6, 1}})
                shortcut.lines[line].side[side].ends = changed;
            require(shortcut.undo() && shortcut.serialize() == initial, "Slope shortcut undo");
            require(shortcut.redo() && shortcut.serialize() != initial, "Slope shortcut redo");

            auto data = encodeSlopes({tilted.slope});
            require(decodeSlopes(data.data(), data.size(), 1)[0] == tilted.slope,
                    "Slope binary round trip");
            rejects([&] { decodeSlopes(data.data(), data.size() - 1, 1); });
            rejects([&] { decodeSlopes(data.data(), data.size(), 2); });
        }
        const auto
 packPath = root / "assets/materials/cyberfort/cyberfort.wad";
        DoomProject project;
        project.loadWad(root / "DOOM.WAD");
        const auto walls = project.textures.size(), flats = project.wad.flatNames().size();
        const auto originalTexture = project.wad.get("TEXTURE1");
        const auto originalFlat = project.wad.get("FLOOR0_1");
        project.loadMaterialPack(packPath);
        require(project.textures.size() == walls + 3, "New wall count");
        require(project.wad.flatNames().size() == flats + 3, "New flat count");
        require(project.wad.get("TEXTURE1") == originalTexture &&
                    project.wad.get("FLOOR0_1") == originalFlat,
                "Original materials must remain unchanged");
        project.loadMaterialPack(packPath);
        require(project.textures.size() == walls + 3, "Loading same pack is idempotent");
        for (const auto &d : project.wad.materials()) {
            Image image;
            if (d.kind == 'F')
                image = project.wad.flat(d.name);
            else {
                const auto t = std::find_if(project.textures.begin(), project.textures.end(),
                                            [&](const Texture &t) { return t.name == d.name; });
                require(t != project.textures.end() && t->width == d.width,
                        "Logical texture width");
                image = project.wad.texture(*t);
            }
            require(image.width > d.width &&
                        image.pixels.size() == size_t(image.width) * image.height,
                    "Decode full-resolution PNG");
        }
        project.maps = {"CYBERFT"};
        project.save(output / "cyberfort.dpr");
        DoomProject restored;
        restored.load(output / "cyberfort.dpr");
        require(restored.materialPacks.size() == 1 && restored.wad.materials().size() == 6 &&
                    restored.textures.size() == project.textures.size(),
                "Project material persis
tence");
        Wadfile resources;
        project.wad.copyMaterialsTo(resources);
        resources.save(output / "resources.wad");
        require(resources.materials().size() == 6, "Portable manifest");
        rejects([&] { project.wad.mergeMaterials(resources); });
        rejects([] { doom_materials::parse("DOOM_MATERIALS 1\nW TOOLONGID 64 64\n"); });
        rejects([] { doom_materials::parse("DOOM_MATERIALS 1\nF NEG -1 64\n"); });
        rejects([] { doom_materials::parse("DOOM_MATERIALS 1\nW SAME 64 64\nF SAME 64 64\n"); });
        for (const auto &name : {"STARTAN3", "FLOOR0_1"}) {
            Wadfile bad;
            const std::string manifest = std::string("DOOM_MATERIALS 1\nW ") + name + " 64 64\n";
            bad.add("DMATDEF", Bytes(manifest.begin(), manifest.end()));
            bad.add(name, resources.get("CWBRICK"));
            rejects([&] { project.wad.mergeMaterials(bad); });
        }
        Wadfile broken;
        const std::string manifest = "DOOM_MATERIALS 1\nW BADPNG 64 64\n";
        broken.add("DMATDEF", Bytes(manifest.begin(), manifest.end()));
        broken.add("BADPNG", Bytes{0, 1, 2});
        const auto before = project.wad.lumps.size();
        rejects([&] { project.wad.mergeMaterials(broken); });
        require(project.wad.lumps.size() == before, "Failed import must leave resources intact");

        // Two connected rooms exercise every material and normal Doom pegging.
        EditWorld world;
        world.clear();
        auto outer = [&](Vec2 a, Vec2 b, const char *wall, bool second) {
            world.newLine(a, b);
            auto &side = world.lines.back().side[0];
            side.midtexture = wall;
            side.ends.floorheight = second ? 16 : 0;
            side.ends.ceilingheight = second ? 112 : 128;
            side.ends.floorflat = second ? "CWGRATE" : "CWFLOOR";
            side.ends.ceilingflat = "CWCEIL";
            side.ends.lightlevel = second ? 192 : 224;
        };
        outer({0, 256}, {256, 2
56}, "CWBRICK", false);
        outer({256, 0}, {0, 0}, "CWSTEEL", false);
        outer({0, 0}, {0, 256}, "CWBRICK", false);
        const auto first = world.lines.back().side[0].ends;
        outer({256, 256}, {512, 256}, "CWSTEEL", true);
        outer({512, 256}, {512, 0}, "CWSERVR", true);
        outer({512, 0}, {256, 0}, "CWSTEEL", true);
        const auto second = world.lines.back().side[0].ends;
        world.newLine({256, 256}, {256, 0});
        auto &portal = world.lines.back();
        portal.flags = ML_TWOSIDED;
        portal.side[0].ends = first;
        portal.side[1].ends = second;
        for (auto &side : portal.side) {
            side.midtexture = "-";
            side.toptexture = side.bottomtexture = "CWSTEEL";
        }
        world.things.push_back({{112, 128}, 0, 1, 7});
        require(connectSectors(world).size() == 2, "Demo must have two sectors");
        world.save(output / "CYBERFT.dwd");
        world.exportMap(output / "editable.wad", "CYBERFT");
        Wadfile map;
        map.load(output / "editable.wad");
        project.wad.copyMaterialsTo(map);
        map.save(output / "editable.wad");
        Wadfile reopened;
        reopened.load(output / "editable.wad");
        EditWorld imported;
        imported.importMap(reopened, "CYBERFT");
        require(imported.lines.size() == world.lines.size() && reopened.materials().size() == 6,
                "Map and materials round trip");
        project.loadMaterialPack(root / "assets/materials/la-night/la-night.wad");
        require(project.wad.flat("LASTORM").width > 1024, "Sky PNG decodes in flat palette");
        auto rotated = world;
        for (auto &line : rotated.lines)
            for (auto &side : line.side) {
                side.ends.rotation = {37125, -22500};
                side.uv = {2000, 500, 7500, -3250};
                side.ends.floorUV = {1500, 2500, 12250, -30000};
                side.ends.ceilingUV = {750, 2000, -9500, 81000};
                side.ends.cei
lingflat = "LASTORM";
            }
        const auto rotationText = rotated.serialize();
        EditWorld rotationRestored;
        rotationRestored.parse(rotationText);
        require(rotationRestored.serialize() == rotationText, "UV rotation DWD round trip");
        rotated.save(output / "CITYSKY.dwd");
        rotated.exportMap(output / "CITYSKY-edit.wad", "CITYSKY");
        Wadfile skyMap;
        skyMap.load(output / "CITYSKY-edit.wad");
        rotationRestored.importMap(skyMap, "CITYSKY");
        require(rotationRestored.lines[0].side[0].ends.rotation == SectorRotation{37125, -22500},
                "Independent floor and ceiling rotations survive WAD round trip");
        require(rotationRestored.lines[0].side[0].uv == TextureUV{2000, 500, 7500, -3250} &&
                    rotationRestored.lines[0].side[0].ends.floorUV ==
                        TextureUV{1500, 2500, 12250, -30000} &&
                    rotationRestored.lines[0].side[0].ends.ceilingUV ==
                        TextureUV{750, 2000, -9500, 81000},
                "Independent wall/floor/ceiling transforms survive WAD round trip");
        auto transformed = TextureUV{2000, 500, 7500, -3250}.apply(64, 32);
        require(std::abs(transformed[0] - 39.5f) < .001f &&
                    std::abs(transformed[1] - 60.75f) < .001f,
                "Texture size/offset convention");
        auto packedUV = encodeTextureUVs({{2000, 500, 7500, -3250}});
        rejects([&] { decodeTextureUVs(packedUV.data(), packedUV.size() - 1, 1); });
        rejects([&] { encodeTextureUVs({{0, 1000, 0, 0}}); });
        require(rotationRestored.points.size() == world.points.size() &&
                    rotationRestored.lines[0].side[0].ends.floorheight == first.floorheight,
                "UV rotation preserves geometry");
        const auto uv = rotatedPlaneUV(64, 0, 90000);
        require(std::abs(uv[0]) < .001f && std::abs(uv[1] - 64) < .001f,
                "Quarter-turn UV coordinates");
       
 const auto encoded = encodeRotations({{37125, -22500}});
        require(decodeRotations(encoded.data(), encoded.size(), 1)[0] ==
                    SectorRotation{37125, -22500},
                "Arbitrary rotation binary round trip");
        rejects([&] { decodeRotations(encoded.data(), encoded.size() - 1, 1); });
        project.wad.copyMaterialsTo(skyMap);
        skyMap.save(output / "CITYSKY-edit.wad");
        std::cout << "Material tests passed; demo project: " << output << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
