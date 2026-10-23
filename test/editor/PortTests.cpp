#include "BlockWorld.h"
#include "DoomProject.h"
#include "idfunctions.h"
#include <algorithm>
#include <iostream>

int main(int argc, char **argv) {
    using namespace de;
    try {
        require(argc == 3, "Usage: doomed_port_tests <project root> <output directory>");
        const auto root = std::filesystem::absolute(argv[1]);
        const auto output = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(output);
        DoomProject project;
        project.loadWad(root / "base/wad/DOOM.WAD");
        project.loadMapTextures(root / "base/textures/e1m1");
        require(!project.textures.empty(), "Map PNG catalog is empty");
        const auto flats = project.wad.flatNames();
        require(flats.size() == project.textures.size(), "Wall/floor catalogs differ");
        for (const auto &texture : project.textures) {
            const auto wall = project.wad.texture(texture);
            const auto floor = project.wad.flat(texture.name);
            require(wall.pixels == floor.pixels && wall.width == floor.width &&
                        wall.height == floor.height,
                    "Cross-assigned PNG preview changed pixels");
        }
        EditWorld world;
        world.load(root / "base/maps/e1m1.dwd");
        const auto serialized = world.serialize();
        EditWorld copy;
        copy.parse(serialized);
        require(copy.serialize() == serialized, "DWD geometry changed during roundtrip");
        require(!connectSectors(copy).empty(), "Map sector connectivity was lost");
        copy.explicitSectors = true;
        copy.reference.path = "reference.png";
        const auto compilerInput = copy.serializeForCompiler();
        require(compilerInput.find("DoomEd sector indices") == std::string::npos &&
                    compilerInput.find("DoomEd reference image") == std::string::npos,
                "Editor metadata leaked into DoomMap input");
        require(copy.explicitSectors && copy.reference.path == "refe
rence.png",
                "Compiler snapshot modified the authoring document");
        copy.parse(serialized);
        bool rejected = false;
        try {
            copy.parse(serialized + "\n// DoomEd sector portals v1\n");
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected && copy.serialize() == serialized,
                "Sector-over-sector rejection modified the current document");
        const auto texturesBefore = project.textures.size();
        const auto directoryBefore = project.wad.textureDirectory;
        const auto badDirectory = output / "bad-png";
        std::filesystem::create_directories(badDirectory);
        writeTextAtomic(badDirectory / "bad.png", "invalid PNG");
        rejected = false;
        try {
            project.loadMapTextures(badDirectory);
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected && project.textures.size() == texturesBefore &&
                    project.wad.textureDirectory == directoryBefore,
                "Malformed PNG load modified the active catalog");
        project.loadMapTextures(output / "missing-map");
        require(project.wad.mapImages.empty(), "Previous map's PNGs leaked into a new map");
        std::cout << "Editor port tests passed: shared PNG catalog/pixels, DWD roundtrip, "
                     "sector rejection, malformed PNG rollback, map switching\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
