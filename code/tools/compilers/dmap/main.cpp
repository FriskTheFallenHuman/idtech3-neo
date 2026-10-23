// SPDX-License-Identifier: GPL-3.0-or-later
#include "doommap.h"
#include "../CompilerRuntime.h"

namespace dm {
Bytes readFile(const fs::path &path) {
    compilerCheckCancelled();
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    require(bool(f), "Cannot open " + path.string());
    auto n = f.tellg();
    require(n >= 0 && n < 1024ll * 1024 * 1024, "Invalid file size: " + path.string());
    Bytes b(size_t(n), 0);
    f.seekg(0);
    if (n)
        f.read(reinterpret_cast<char *>(b.data()), n);
    require(bool(f), "Cannot read " + path.string());
    return b;
}
void writeFile(const fs::path &path, const Bytes &bytes) {
    compilerCheckCancelled();
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    require(bool(f), "Cannot write " + path.string());
    f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    require(bool(f), "Write failed: " + path.string());
}
std::string upper(std::string s) {
    for (char &c : s)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string lower(std::string s) {
    for (char &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string lumpName(const uint8_t *p, size_t n) {
    size_t l = 0;
    while (l < n && p[l])
        ++l;
    return upper(std::string(reinterpret_cast<const char *>(p), l));
}
} // namespace dm
int DmapMain(int argc, char **argv) {
    using namespace dm;
    try {
        const auto &root = compilerProjectRoot();
        fs::path base = root / "base", wadPath, textureRoot;
        bool all = false, strict = false;
        std::vector<fs::path> inputs;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--help" || a == "-h") {
                std::cout << "dmap [--base <base directory>] [--wad <IWAD>] "
                             "[--textures <texture root>] [--strict] "
                             "<map.dwd>...\n        dmap [--base <base directory>] [--wad "
                             "<IWAD>] --all\nOutputs base/maps/<map>.bsp, "
                             "base/textures/<map>/*.png, base/scripts/<map>.shader\n";
                return 0;
            }
            if (a == "--base" || a == "--wad" || a == "--textures") {
                require(i + 1 < argc, "Missing value for " + a);
                (a == "--base"  ? base
                 : a == "--wad" ? wadPath
                                : textureRoot) = fs::absolute(argv[++i]);
            } else if (a == "--all")
                all = true;
            else if (a == "--strict")
                strict = true;
            else {
                require(a.empty() || a[0] != '-', "Unknown option: " + a);
                inputs.emplace_back(a);
            }
        }
        base = fs::absolute(base);
        if (wadPath.empty()) {
            auto dir = base / "wad";
            require(fs::is_directory(dir), "Missing WAD directory: " + dir.string());
            for (auto &e : fs::directory_iterator(dir))
                if (e.is_regular_file() && lower(e.path().extension().string()) == ".wad") {
                    require(wadPath.empty(), "Multiple WADs found; select one with --wad");
                    wadPath = e.path();
                }
        }
        require(!wadPath.empty(), "No WAD found; use --wad");
        if (all)
            for (auto &e : fs::directory_iterator(base / "maps"))
                if (lower(e.path().extension().string()) == ".dwd")
                    inputs.push_back(e.path());
        require(!inputs.empty(), "Specify a .dwd map or --all; use --help for usage");
        std::sort(inputs.begin(), inputs.end());
        inputs.erase(std::unique(inputs.begin(), inputs.end()), inputs.end());
        Wad wad(wadPath);
        int failures = 0;
        for (auto path : inputs)
            try {
                if (!fs::exists(path))
                    path = base / "maps" / path;
                require(lower(path.extension().string()) == ".dwd",
                        "Only WorldServer v4 .dwd sources are supported");
                Map map = loadDwd(path, strict);
                Assets assets{map.name, base, wad, {}, {}, {}};
                if (!textureRoot.empty()) {
                    assets.sourceTextures = textureRoot / map.name;
                }
                auto result = compile(map, assets, base / "maps" / (map.name + ".bsp"));
                assets.write();
                std::ofstream report(base / "maps" / (map.name + ".doommap.txt"), std::ios::trunc);
                report << "dmap IBSP " << dbsp::Version << "\nsource "
                       << fs::absolute(path).string() << "\nwad " << wadPath.string()
                       << "\nsectors " << result.sectors << "\nlines " << result.lines << "\ncells "
                       << result.cells << "\nmodels " << result.models << "\nsurfaces "
                       << result.surfaces << "\nbrushes " << result.brushes << "\ntextures "
                       << assets.images.size() << "\nlightmaps 0\n";
                for (auto &w : map.warnings) {
                    report << "WARNING " << w << '\n';
                    std::cerr << map.name << ": warning: " << w << '\n';
                }
                require(bool(report), "Cannot write compilation report");
                std::cout << map.name << ": " << result.sectors << " sectors, " << result.models - 1
                          << " entity models, " << result.surfaces << " surfaces, "
                          << assets.images.size() << " PNGs\n";
            } catch (const std::exception &e) {
                ++failures;
                std::cerr << path.string() << ": ERROR: " << e.what() << '\n';
            }
        return failures ? 1 : 0;
    } catch (const std::exception &e) {
        std::cerr << "dmap: " << e.what() << '\n';
        return 1;
    }
}
