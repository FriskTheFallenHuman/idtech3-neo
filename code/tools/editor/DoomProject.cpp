#include "DoomProject.h"
#include "idfunctions.h"
#include <algorithm>
#include <fstream>
#include <sstream>
namespace de {
void DoomProject::loadWad(const std::filesystem::path &p) {
    Wadfile next;
    next.load(p);
    auto t = next.surfaceTextures();
    wad = std::move(next);
    textures = std::move(t);
    wadpath = std::filesystem::absolute(p).lexically_normal();
    materialPacks.clear();
}
void DoomProject::loadMaterialPack(const std::filesystem::path &path) {
    const auto absolute = std::filesystem::weakly_canonical(path);
    if (std::find(materialPacks.begin(), materialPacks.end(), absolute) != materialPacks.end())
        return;
    Wadfile pack;
    pack.load(absolute);
    for (const auto &d : pack.materials()) {
        require(std::none_of(textures.begin(), textures.end(),
                             [&](const Texture &t) { return t.name == d.name; }),
                "Material collides with project texture: " + d.name);
    }
    wad.mergeMaterials(pack);
    textures = wad.surfaceTextures();
    materialPacks.push_back(absolute);
}
void DoomProject::load(const std::filesystem::path &p) {
    auto b = readFile(p);
    std::istringstream in(std::string(b.begin(), b.end()));
    std::string row;
    std::getline(in, row);
    require(row.find("Doom Project version 1") == 0, "Unsupported project version");
    DoomProject result;
    result.pathname = p;
    int count = -1;
    std::vector<std::filesystem::path> packs;
    auto resolve = [&](std::string v) {
        auto path = std::filesystem::u8path(v);
        return path.is_absolute() ? path : p.parent_path() / path;
    };
    while (std::getline(in, row)) {
        if (!row.empty() && row.back() == '\r')
            row.pop_back();
        auto colon = row.find(':');
        if (colon == std::string::npos)
            continue;
        auto key = row.substr(0, colon), value = row.substr(colon + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        if (key == "wadfile")
            result.wadpath = resolve(value);
        else if (key == "materialpack")
            packs.push_back(resolve(value));
        else if (key == "mapwads")
            result.mapwads = resolve(value);
        else if (key == "BSPprogram")
            result.bspProgram = value == "-" ? std::filesystem::path{} : resolve(value);
        else if (key == "BSPhost")
            result.bspHost = value;
        else if (key == "nummaps") {
            count = std::stoi(value);
            require(count >= 0 && count <= 10000, "Invalid project map count");
            for (int i = 0; i < count; ++i) {
                require(bool(std::getline(in, row)), "Truncated project maps");
                if (!row.empty() && row.back() == '\r')
                    row.pop_back();
                require(!row.empty() && row.size() <= 8, "Invalid map name");
                result.maps.push_back(row);
            }
        }
    }
    require(count >= 0, "Missing project maps");
    if (!result.wadpath.empty() && std::filesystem::exists(result.wadpath))
        result.loadWad(result.wadpath);
    for (int i = 0; i < 2; ++i) {
        auto f = p.parent_path() / ("texture" + std::to_string(i + 1) + ".dsp");
        if (std::filesystem::exists(f))
            result.loadTextures(f, i);
    }
    for (const auto &pack : packs)
        result.loadMaterialPack(pack);
    *this = std::move(result);
}
void DoomProject::save(const std::filesystem::path &p) {
    std::ostringstream out;
    out << "Doom Project version 1\n\nwadfile: " << wadpath.u8string()
        << "\nmapwads: " << (mapwads.empty() ? p.parent_path().u8string() : mapwads.u8string())
        << "\nBSPprogram: " << (bspProgram.empty() ? "-" : bspProgram.u8string())
        << "\nBSPhost: " << bspHost << "\nnummaps: " << maps.size() << '\n';
    for (auto &n : maps)
        out << n << '\n';
    for (const auto &pack : materialPacks) {
        auto relative = pack.lexically_relative(std::filesystem::absolute(p).parent_path());
        out << "materialpack: " << (relative.empty() ? pack : relative).u8string() << '\n';
    }
    writeTextAtomic(p, out.str());
    for (int i = 0; i < 2; ++i)
        saveTextures(p.parent_path() / ("texture" + std::to_string(i + 1) + ".dsp"), i);
    pathname = p;
}
void DoomProject::loadTextures(const std::filesystem::path &path, int set) {
    auto b = readFile(path);
    std::istringstream in(std::string(b.begin(), b.end()));
    std::string row;
    std::getline(in, row);
    int count = 0;
    require(sscanf(row.c_str(), "numtextures: %d", &count) == 1 && count >= 0 && count <= 100000,
            "Invalid texture count");
    std::vector<Texture> list;
    for (int i = 0; i < count; ++i) {
        std::getline(in, row);
        Texture t;
        t.wadIndex = set;
        char name[9]{};
        int patches = 0;
        require(sscanf(row.c_str(), "%8s %d, %d, %d", name, &t.width, &t.height, &patches) == 4 &&
                    patches >= 0 && patches <= 4096,
                "Malformed texture");
        t.name = name;
        for (int j = 0; j < patches; ++j) {
            std::getline(in, row);
            Patch p;
            require(sscanf(row.c_str(), " (%d, %d : %8s ) %d, %d", &p.x, &p.y, name, &p.stepdir,
                           &p.colormap) == 5,
                    "Malformed texture patch");
            p.name = name;
            t.patches.push_back(p);
        }
        list.push_back(t);
    }
    textures.erase(std::remove_if(textures.begin(), textures.end(),
                                  [&](auto &t) { return t.wadIndex == set; }),
                   textures.end());
    textures.insert(textures.end(), list.begin(), list.end());
}
void DoomProject::saveTextures(const std::filesystem::path &path, int set) const {
    std::ostringstream out;
    int n = 0;
    for (auto &t : textures)
        if (t.wadIndex == set)
            ++n;
    out << "numtextures: " << n << '\n';
    for (auto &t : textures)
        if (t.wadIndex == set) {
            out << t.name << ' ' << t.width << ", " << t.height << ", " << t.patches.size() << '\n';
            for (auto &p : t.patches)
                out << "   (" << p.x << ", " << p.y << " : " << p.name << " ) " << p.stepdir << ", "
                    << p.colormap << '\n';
        }
    writeTextAtomic(path, out.str());
}
void DoomProject::exportTextures(const std::filesystem::path &path) const {
    Wadfile out;
    std::vector<std::string> names;
    for (auto &t : textures) {
        if (t.wadIndex == 2 || t.wadIndex == 3)
            continue;
        require(t.width > 0 && t.height > 0 && t.width <= 4096 && t.height <= 4096,
                "Invalid dimensions for texture " + t.name);
        require(t.patches.size() <= 32767, "Too many patches in texture " + t.name);
        require(t.wadIndex == 0 || t.wadIndex == 1, "Invalid texture set");
    }
    for (auto &t : textures)
        for (auto &p : t.patches)
            if (std::find(names.begin(), names.end(), p.name) == names.end())
                names.push_back(p.name);
    Bytes pn;
    put32(pn, uint32_t(names.size()));
    for (auto &n : names)
        putName(pn, n);
    out.add("PNAMES", pn);
    for (int set = 0; set < 2; ++set) {
        std::vector<const Texture *> ts;
        for (auto &t : textures)
            if (t.wadIndex == set)
                ts.push_back(&t);
        if (ts.empty())
            continue;
        Bytes b;
        put32(b, uint32_t(ts.size()));
        b.resize(4 + 4 * ts.size());
        for (size_t i = 0; i < ts.size(); ++i) {
            uint32_t off = uint32_t(b.size());
            for (int j = 0; j < 4; ++j)
                b[4 + 4 * i + j] = uint8_t(off >> (j * 8));
            auto &t = *ts[i];
            putName(b, t.name);
            put32(b, 0);
            put16(b, t.width);
            put16(b, t.height);
            put32(b, 0);
            put16(b, int(t.patches.size()));
            for (auto &p : t.patches) {
                put16(b, p.x);
                put16(b, p.y);
                put16(b, int(std::find(names.begin(), names.end(), p.name) - names.begin()));
                put16(b, p.stepdir);
                put16(b, p.colormap);
            }
        }
        out.add(set ? "TEXTURE2" : "TEXTURE1", b);
    }
    out.saveWithMaterials(path, wad);
}
} // namespace de
