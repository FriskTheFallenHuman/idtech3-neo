#include "Wadfile.h"
#include "idfunctions.h"
#include <cstdlib>
#include <new>
#include <iostream>

// Reproduce a fragmented editor heap: packaging may allocate small metadata,
// but cannot obtain another large contiguous copy of a material or full WAD.
static bool restrictAllocations = false;
void *operator new(std::size_t size) {
    if (restrictAllocations && size > 1024 * 1024)
        throw std::bad_alloc();
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept {
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}

int main(int argc, char **argv) {
    try {
        using namespace de;
        require(argc == 3, "expected root and output directory");
        const std::filesystem::path root = argv[1], out = argv[2];
        std::filesystem::create_directories(out);
        Wadfile resources, map;
        auto png = readFile(root / "assets/models/traffic-signal/red.png");
        png.resize(32 * 1024 * 1024, 0); // Valid lens PNG with trailing test payload.
        resources.add("BIGSKIN", std::move(png));
        const std::string manifest =
            "DOOM_MATERIALS 1\nW BIGSKIN 64 64 EMISSIVE 1 0 0 5 420 BLINK 35 0\n";
        resources.add("DMATDEF", Bytes(manifest.begin(), manifest.end()));
        map.add("TESTMAP", {});
        map.add("THINGS", Bytes{1, 2, 3, 4});
        restrictAllocations = true;
        bool oldCopyFailed = false;
        try {
            auto copied = map;
            resources.copyMaterialsTo(copied);
        } catch (const std::bad_alloc &) {
            oldCopyFailed = true;
        }
        require(oldCopyFailed, "fixture must reject duplicate material allocations");
        map.saveWithMaterials(out / "streamed.wad", resources);
        resources.save(out / "resources.wad");
        restrictAllocations = false;
        Wadfile loaded;
        loaded.load(out / "streamed.wad");
        require(loaded.
lumps.size() == 4 && loaded.get("THINGS") == map.get("THINGS"),
                "map records preserved");
        require(loaded.get("BIGSKIN") == resources.get("BIGSKIN"),
                "streamed material bytes preserved");
        require(loaded.materials()[0].emission.blink, "animated material metadata preserved");
        // Existing destinations survive validation errors before the atomic write.
        const auto original = readFile(out / "streamed.wad");
        map.add("BIGSKIN", Bytes{0});
        bool rejected = false;
        try {
            map.saveWithMaterials(out / "streamed.wad", resources);
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected && readFile(out / "streamed.wad") == original,
                "failed packaging preserves destination");
        require(map.lumps.size() == 3 && resources.lumps.size() == 2,
                "packaging does not mutate inputs");
        std::cout << "WAD streaming passed: 32 MiB material with 1 MiB allocation limit, byte "
                     "preservation, atomic failure\n";
        return 0;
    } catch (const std::exception &e) {
        restrictAllocations = false;
        std::cerr << e.what() << '\n';
        return 1;
    }
}
