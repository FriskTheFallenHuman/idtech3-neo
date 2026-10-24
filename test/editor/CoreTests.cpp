#include "BlockWorld.h"
#include "DoomProject.h"
#include "EditWorld.h"
#include "idfunctions.h"
#include "lbmfunctions.h"
#include "pathops.h"
#include <cmath>
#include <iostream>
using namespace de;
static int checks = 0;
static void check(bool b, const char *s) {
    ++checks;
    if (!b)
        throw std::runtime_error(s);
}
template <class F> static void rejects(F f, const char *s) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception &) {
        threw = true;
    }
    check(threw, s);
}
static EditWorld room(float x = 0) {
    EditWorld w;
    w.clear();
    w.newLine({x, 128}, {x + 128, 128});
    w.newLine({x + 128, 128}, {x + 128, 0});
    w.newLine({x + 128, 0}, {x, 0});
    w.newLine({x, 0}, {x, 128});
    w.things.push_back({{x + 64, 64}, 90, 1, 7});
    return w;
}
int main(int argc, char **argv) {
    try {
        require(argc == 3, "Expected WAD and output directory");
        std::filesystem::path out = argv[2];
        std::filesystem::create_directories(out);
        auto w = room();
        check(w.dirty(), "Created room should be dirty");
        check(w.validate().empty(), "Room validation");
        auto sectors = connectSectors(w);
        check(sectors.size() == 1 && sectors[0].sides.size() == 4, "Room topology");
        auto text = w.serialize();
        EditWorld copy;
        copy.parse(text);
        check(copy.serialize() == text, "Text round trip");
        check(!copy.dirty(), "Loaded map clean");
        copy.lines[0].selected = 1;
        check(!copy.dirty(), "Selection must not mark map dirty");
        copy.checkpoint();
        copy.moveSelected({16, 0});
        check(copy.dirty(), "Move dirty");
        check(copy.undo() && copy.serialize() == text, "Undo movement");
        check(copy.redo() && copy.dirty(), "Redo movement");
        copy.undo();
        copy.copy();
        copy.paste({256, 256});
        check(copy.lines.size() == 5, "Clipboard paste");
        copy.undo();
        check(copy.lines.size() == 4, "Undo paste");
        copy.splitLine(0, {64, 128});
        check(copy.lines.size() == 5 && connectSectors(copy).size() == 1,
              "Split line preserves topology");
        copy.undo();
        copy.points[0].selected = 1;
        copy.deleteSelected();
        check(copy.lines[0].selected == -1 && copy.lines[3].selected == -1,
              "Deleting vertex deletes incident lines");
        copy.undo();
        rejects([&] { copy.parse("WorldServer version 4\nlines:1\n"); }, "Truncated map must fail");
        check(copy.serialize() == text, "Failed parse preserves map");
        w.save(out / "room.dwd");
        EditWorld loaded;
        loaded.load(out / "room.dwd");
        check(loaded.serialize() == text, "File round trip");
        w.exportMap(out / "room.wad", "E1M1");
        Wadfile generated;
        generated.load(out / "room.wad");
        loaded.importMap(generated, "E1M1");
        loaded.explicitSectors = false;
        check(loaded.serialize() == text, "Retail WAD export/import");
        auto raised = room();
        raised.things.push_back({{32, 32}, 0, 71, 7, 1});
        raised.things.push_back({{96, 96}, 0, 2011, 7, 1});
        const auto originalThings = raised.serialize();
        check(raised.moveSelectedThingsVertically(8), "Selected things raise");
        check(raised.things[0].height == 0 && raised.things[1].height == 8 &&
                  raised.things[2].height == 8,
              "Height edits affect only selected things");
        check(raised.things[1].origin == Vec2{32, 32}, "Height edits preserve XY position");
        check(raised.undo() && raised.serialize() == originalThings, "Thing height undo");
        check(raised.redo() && raised.things[1].height == 8, "Thing height redo");
        raised.moveSelectedThingsVertically(-1);
        check(raised.things[1].height == 7, "Fine downward height step");
        EditWorld heightReload;
        heightReload.parse(raised.serialize());
        check(heightReload.things[1].height == 7 && heightReload.things[2].height == 7,
              "DWD preserves thing heights");
        raised.exportMap(out / "heights.wad", "E1M1");
        Wadfile heightWad;
        heightWad.load(out / "heights.wad");
        heightReload.importMap(heightWad, "E1M1");
        check(heightReload.things[0].height == 0 && heightReload.things[1].height == 7 &&
                  heightReload.things[2].height == 7,
              "WAD round trip preserves THINGS height order");
        check(heightWad.get("THINGS").size() == 30,
              "Thing heights retain classic THINGS record size");
        rejects(
            [&] {
                decodeThingHeights(heightWad.get("DTHINGZ").data(), heightWad.get("DTHINGZ").size(),
                                   4);
            },
            "Height metadata rejects a mismatched thing count");
        auto open = room();
        open.lines.back().selected = -1;
        rejects([&] { connectSectors(open); }, "Open room rejected");
        auto mismatch = room();
        mismatch.lines[0].side[0].ends.lightlevel = 20;
        rejects([&] { connectSectors(mismatch); }, "Sector mismatch rejected");
        auto disjoint = room();
        auto other = room(256);
        for (auto &l : other.lines)
            disjoint.newLine(other.points[l.p1].pt, other.points[l.p2].pt, l);
        check(connectSectors(disjoint).size() == 2, "Disconnected rooms remain separate");
        auto island = room();
        island.newLine({32, 32}, {96, 32});
        island.newLine({96, 32}, {96, 96});
        island.newLine({96, 96}, {32, 96});
        island.newLine({32, 96}, {32, 32});
        check(connectSectors(island).size() == 1, "Island hole connects to enclosing sector");
        auto nan = room();
        rejects([&] { nan.newPoint({NAN, 0}); }, "NaN rejected");
        Bytes invalid = {'P', 'W', 'A', 'D'};
        put32(invalid, 0xffffffff);
        put32(invalid, 12);
        writeFileAtomic(out / "bad.wad", invalid);
        rejects([&] { generated.load(out / "bad.wad"); }, "Invalid WAD directory rejected");
        check(generated.mapNames().size() == 1, "Failed WAD read preserves previous data");
        check(lineInRect({-2, 0}, {2, 0}, {-1, -1}, {1, 1}), "Line crossing rectangle");
        check(!lineInRect({2, 2}, {3, 3}, {-1, -1}, {1, 1}), "Line outside rectangle");
        Bytes pixels(65 * 3), pal(768);
        for (size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = uint8_t(i);
        for (int i = 0; i < 256; ++i) {
            pal[i * 3] = uint8_t(i);
            pal[i * 3 + 1] = uint8_t(255 - i);
        }
        saveLBM(out / "test.lbm", pixels, 65, 3, pal);
        auto image = loadLBM(out / "test.lbm");
        check(image.width == 65 && image.height == 3 &&
                  image.pixels[64] == (0xff000000u | 64u | (191u << 8)),
              "Odd-width LBM roundtrip");
        // The real-WAD section needs DOOM.WAD; CI has no game data, so run
        // only the self-contained checks when the file is absent.
        const bool hasWad = std::filesystem::exists(std::filesystem::u8path(argv[1]));
        size_t mapCount = 0, mapLines = 0, textureCount = 0;
        if (hasWad) {
            Wadfile wad;
            wad.load(std::filesystem::u8path(argv[1]));
            auto maps = wad.mapNames();
            check(!maps.empty(), "Real WAD contains maps");
            mapCount = maps.size();
            for (auto &name : maps) {
                EditWorld map;
                map.importMap(wad, name);
                EditWorld reload;
                reload.parse(map.serialize());
                check(reload.serialize() == map.serialize(), "Real map text roundtrip");
                mapLines += map.lines.size();
                try {
                    auto connected = connectSectors(reload);
                    check(!connected.empty(), "Real map sector detection");
                    reload.exportMap(out / (name + ".wad"), name);
                } catch (const std::exception &e) {
                    std::cerr << name << ": " << e.what() << "\n";
                    throw;
                }
            }
            auto textures = wad.textures();
            check(!textures.empty(), "Real textures decoded");
            textureCount = textures.size();
            for (auto &t : textures) {
                auto im = wad.texture(t);
                check(im.pixels.size() == size_t(t.width) * t.height, "Texture composition size");
            }
            for (auto &name : wad.flatNames())
                check(wad.flat(name).pixels.size() == 4096, "Flat decoding");
            DoomProject project;
            project.loadWad(std::filesystem::u8path(argv[1]));
            project.maps = {"E1M1"};
            project.save(out / "project.dpr");
            DoomProject project2;
            project2.load(out / "project.dpr");
            check(project2.textures.size() == project.textures.size() && project2.maps == project.maps,
                  "Project roundtrip");
            project.exportTextures(out / "textures.wad");
            Wadfile tw;
            tw.load(out / "textures.wad");
            check(tw.textures().size() == textures.size(), "Texture WAD roundtrip");
            w.exportLegacy(out / "legacy.wad", "E1M1", textures);
            Wadfile legacy;
            legacy.load(out / "legacy.wad");
            check(u32(legacy.get("POINTS"), 0) == 4 && u32(legacy.get("LINES"), 0) == 4 &&
                      u32(legacy.get("SECTORS"), 0) == 1,
                  "1993 WAD export counts");
        }
        std::cout << "PASS: " << checks << " checks";
        if (hasWad)
            std::cout << "; " << mapCount << " real maps, " << mapLines << " linedefs, "
                      << textureCount << " textures
";
        else
            std::cout << "; real WAD checks skipped (" << argv[1] << " not available)
";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
