// SPDX-License-Identifier: GPL-3.0-or-later
#include "doommap.h"
#include <png.h>

namespace dm {
namespace {
uint32_t u32(const Bytes &b, size_t p) {
    require(p <= b.size() && b.size() - p >= 4, "Truncated WAD data");
    return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) |
           (uint32_t(b[p + 3]) << 24);
}
uint16_t u16(const Bytes &b, size_t p) {
    require(p <= b.size() && b.size() - p >= 2, "Truncated WAD data");
    return uint16_t(b[p]) | uint16_t(b[p + 1] << 8);
}
void be32(Bytes &b, uint32_t n) {
    b.push_back(uint8_t(n >> 24));
    b.push_back(uint8_t(n >> 16));
    b.push_back(uint8_t(n >> 8));
    b.push_back(uint8_t(n));
}
uint32_t crc(const uint8_t *p, size_t n) {
    uint32_t c = ~0u;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; ++i)
            c = (c >> 1) ^ ((c & 1) ? 0xedb88320u : 0);
    }
    return ~c;
}
void chunk(Bytes &b, const char *type, const Bytes &data) {
    be32(b, uint32_t(data.size()));
    size_t start = b.size();
    b.insert(b.end(), type, type + 4);
    b.insert(b.end(), data.begin(), data.end());
    be32(b, crc(b.data() + start, b.size() - start));
}
const std::vector<std::vector<std::string>> animations = {
    {"NUKAGE1", "NUKAGE2", "NUKAGE3"},
    {"FWATER1", "FWATER2", "FWATER3", "FWATER4"},
    {"SWATER1", "SWATER2", "SWATER3", "SWATER4"},
    {"LAVA1", "LAVA2", "LAVA3", "LAVA4"},
    {"BLOOD1", "BLOOD2", "BLOOD3"},
    {"BLODGR1", "BLODGR2", "BLODGR3", "BLODGR4"},
    {"SLADRIP1", "SLADRIP2", "SLADRIP3"},
    {"BLODRIP1", "BLODRIP2", "BLODRIP3", "BLODRIP4"},
    {"FIREWALA", "FIREWALB", "FIREWALL"},
    {"GSTFONT1", "GSTFONT2", "GSTFONT3"},
    {"FIRELAV3", "FIRELAVA"},
    {"FIREMAG1", "FIREMAG2", "FIREMAG3"},
    {"FIREBLU1", "FIREBLU2"},
    {"ROCKRED1", "ROCKRED2", "ROCKRED3"},
    {"WFALL1", "WFALL2", "WFALL3", "WFALL4"}};
} // namespace
void writePng(const fs::path &path, const Image &im) {
    require(im.width > 0 && im.height > 0 && im.pixels.size() == size_t(im.width) * im.height * 4,
            "Invalid PNG image");
    Bytes png = {137, 80, 78, 71, 13, 10, 26, 10}, ihdr;
    be32(ihdr, im.width);
    be32(ihdr, im.height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk(png, "IHDR", ihdr);
    Bytes raw;
    raw.reserve(im.pixels.size() + im.height);
    for (int y = 0; y < im.height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), im.pixels.begin() + size_t(y) * im.width * 4,
                   im.pixels.begin() + size_t(y + 1) * im.width * 4);
    }
    Bytes z = {0x78, 0x01};
    for (size_t start = 0; start < raw.size();) {
        size_t n = std::min(size_t(65535), raw.size() - start);
        z.push_back(start + n == raw.size() ? 1 : 0);
        z.push_back(uint8_t(n));
        z.push_back(uint8_t(n >> 8));
        uint16_t inv = uint16_t(~n);
        z.push_back(uint8_t(inv));
        z.push_back(uint8_t(inv >> 8));
        z.insert(z.end(), raw.begin() + start, raw.begin() + start + n);
        start += n;
    }
    uint32_t a = 1, b = 0;
    for (auto c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    be32(z, (b << 16) | a);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    writeFile(path, png);
}
Wad::Wad(const fs::path &path) : data(readFile(path)) {
    require(data.size() >= 12 &&
                (memcmp(data.data(), "IWAD", 4) == 0 || memcmp(data.data(), "PWAD", 4) == 0),
            "Not a Doom WAD");
    auto count = u32(data, 4), dir = u32(data, 8);
    require(count <= 1000000 && dir <= data.size() && uint64_t(count) * 16 <= data.size() - dir,
            "Invalid WAD directory");
    bool flat = false;
    for (uint32_t i = 0; i < count; ++i) {
        size_t p = dir + size_t(i) * 16;
        Entry e{u32(data, p), u32(data, p + 4), lumpName(data.data() + p + 8), flat};
        require(e.offset <= data.size() && e.size <= data.size() - e.offset,
                "WAD lump outside file");
        if (e.name == "F_START" || e.name == "FF_START")
            flat = true;
        if (e.name == "F_END" || e.name == "FF_END")
            flat = false;
        e.flat = flat && e.size == 4096;
        lookup[e.name] = int(entries.size());
        entries.push_back(e);
    }
    auto find = [&](const char *name) -> const Entry & {
        auto i = lookup.find(name);
        require(i != lookup.end(), std::string("WAD missing ") + name);
        return entries[i->second];
    };
    auto &pal = find("PLAYPAL");
    require(pal.size >= 768, "Short PLAYPAL");
    std::copy_n(data.data() + pal.offset, 768, palette.begin());
    auto &names = find("PNAMES");
    uint32_t n = u32(data, names.offset);
    require(uint64_t(n) * 8 + 4 <= names.size, "Short PNAMES");
    for (uint32_t i = 0; i < n; ++i)
        patchNames.push_back(lumpName(data.data() + names.offset + 4 + i * 8));
    for (auto label : {"TEXTURE1", "TEXTURE2"}) {
        auto it = lookup.find(label);
        if (it == lookup.end())
            continue;
        auto &e = entries[it->second];
        uint32_t num = u32(data, e.offset);
        require(uint64_t(num) * 4 + 4 <= e.size, "Bad texture directory");
        for (uint32_t i = 0; i < num; ++i) {
            uint32_t off = u32(data, e.offset + 4 + i * 4);
            require(off <= e.size && e.size - off >= 22, "Bad texture offset");
            size_t p = e.offset + off;
            Texture t;
            t.width = u16(data, p + 12);
            t.height = u16(data, p + 14);
            uint16_t pc = u16(data, p + 20);
            require(t.width > 0 && t.height > 0 && t.width <= 4096 && t.height <= 4096 &&
                        uint64_t(pc) * 10 + 22 <= e.size - off,
                    "Invalid texture dimensions/patches");
            for (uint16_t j = 0; j < pc; ++j) {
                size_t q = p + 22 + j * 10;
                Texture::Patch patch{int(int16_t(u16(data, q))), int(int16_t(u16(data, q + 2))),
                                     int(u16(data, q + 4))};
                require(size_t(patch.index) < patchNames.size(), "Invalid texture PNAMES index");
                t.patches.push_back(patch);
            }
            textures[lumpName(data.data() + p)] = t;
        }
    }
}
bool Wad::has(const std::string &name, bool flat) const {
    auto n = upper(name);
    if (!flat)
        return textures.count(n) != 0;
    auto it = lookup.find(n);
    return it != lookup.end() && entries[it->second].flat;
}
Image Wad::patch(const std::string &name) const {
    auto it = lookup.find(name);
    require(it != lookup.end(), "Missing WAD patch: " + name);
    auto &e = entries[it->second];
    require(e.size >= 8, "Short WAD patch: " + name);
    Image im;
    im.width = u16(data, e.offset);
    im.height = u16(data, e.offset + 2);
    require(im.width > 0 && im.height > 0 && im.width <= 4096 && im.height <= 4096 &&
                8ull + im.width * 4ull <= e.size,
            "Bad patch: " + name);
    im.pixels.resize(size_t(im.width) * im.height * 4);
    for (int x = 0; x < im.width; ++x) {
        uint32_t off = u32(data, e.offset + 8 + x * 4);
        require(off < e.size, "Patch column outside lump");
        size_t p = e.offset + off, end = e.offset + e.size;
        int previous = -1;
        for (;;) {
            require(p < end, "Unterminated patch column");
            int top = data[p++];
            if (top == 255)
                break;
            require(end - p >= 2, "Short patch post");
            int length = data[p++];
            ++p;
            if (top <= previous)
                top += previous;
            previous = top;
            require(size_t(length) + 1 <= end - p, "Patch post outside lump");
            for (int j = 0; j < length; ++j) {
                int y = top + j;
                if (y >= 0 && y < im.height) {
                    size_t dest = (size_t(y) * im.width + x) * 4;
                    int color = data[p + j];
                    std::copy_n(palette.data() + color * 3, 3, im.pixels.data() + dest);
                    im.pixels[dest + 3] = 255;
                }
            }
            p += length + 1;
        }
    }
    return im;
}
Image Wad::image(const std::string &name, bool flat) const {
    std::string n = upper(name);
    require(has(n, flat), std::string("WAD missing ") + (flat ? "flat " : "wall texture ") + n);
    Image im;
    if (flat) {
        auto &e = entries.at(lookup.at(n));
        im.width = im.height = 64;
        im.pixels.resize(64 * 64 * 4);
        for (size_t i = 0; i < 4096; ++i) {
            std::copy_n(palette.data() + data[e.offset + i] * 3, 3, im.pixels.data() + i * 4);
            im.pixels[i * 4 + 3] = 255;
        }
    } else {
        auto &t = textures.at(n);
        im.width = t.width;
        im.height = t.height;
        im.pixels.resize(size_t(im.width) * im.height * 4);
        for (auto &p : t.patches) {
            auto pic = patch(patchNames[p.index]);
            for (int y = 0; y < pic.height; ++y)
                for (int x = 0; x < pic.width; ++x) {
                    int dx = p.x + x, dy = p.y + y;
                    if (dx < 0 || dx >= im.width || dy < 0 || dy >= im.height)
                        continue;
                    auto src = pic.pixels.data() + (size_t(y) * pic.width + x) * 4;
                    if (src[3])
                        std::copy_n(src, 4, im.pixels.data() + (size_t(dy) * im.width + dx) * 4);
                }
        }
    }
    for (size_t i = 3; i < im.pixels.size(); i += 4)
        if (im.pixels[i] != 255) {
            im.masked = true;
            break;
        }
    return im;
}
std::vector<std::string> Wad::animation(const std::string &n, bool flat) const {
    for (auto &group : animations)
        if (std::find(group.begin(), group.end(), n) != group.end()) {
            bool complete = true;
            for (auto &x : group)
                complete &= has(x, flat);
            if (complete)
                return group;
        }
    return {n};
}
Image Assets::image(const std::string &name, bool flat) const {
    if (!sourceTextures.empty()) {
        const auto path = sourceTextures / (lower(name) + ".png");
        if (fs::is_regular_file(path)) {
            const auto bytes = readFile(path);
            png_image png{};
            png.version = PNG_IMAGE_VERSION;
            struct PngScope {
                png_image &value;
                ~PngScope() { png_image_free(&value); }
            } scope{png};
            require(png_image_begin_read_from_memory(&png, bytes.data(), bytes.size()) != 0,
                    "Cannot read PNG " + path.string() + ": " + png.message);
            require(png.width && png.height && png.width <= 4096 && png.height <= 4096,
                    "PNG dimensions must be 1..4096: " + path.string());
            png.format = PNG_FORMAT_RGBA;
            Image result;
            result.width = static_cast<int>(png.width);
            result.height = static_cast<int>(png.height);
            result.pixels.resize(PNG_IMAGE_SIZE(png));
            require(png_image_finish_read(&png, nullptr, result.pixels.data(), 0, nullptr) != 0,
                    "Cannot decode PNG " + path.string() + ": " + png.message);
            for (size_t i = 3; i < result.pixels.size(); i += 4) {
                result.masked |= result.pixels[i] != 255;
            }
            return result;
        }
    }
    // Storage in a WAD flat lump or patch composition does not constrain use.
    return wad.image(name, !wad.has(name, false));
}

std::string Assets::texture(const std::string &name, bool flat) {
    if (name == "-" || name.empty())
        return {};
    auto keyFor = [](const std::string &n) { return n; };
    auto add = [&](const std::string &n) {
        auto key = keyFor(n);
        if (images.count(key))
            return;
        images[key] = image(n, flat);
        flats[key] = flat;
    };
    for (auto &n : wad.animation(name, flat))
        add(n);
    if (!flat && name.size() > 3 && (name.substr(0, 3) == "SW1" || name.substr(0, 3) == "SW2")) {
        auto alternate = name;
        alternate[2] = name[2] == '1' ? '2' : '1';
        if (wad.has(alternate, false))
            add(alternate);
    }
    return "textures/" + mapName + "/" + lower(keyFor(name));
}

void Assets::write() {
    // Doom skies are cylindrical strips. Resample to the engine's six cube faces.
    if (images.count("F_SKY1")) {
        int episode = mapName.size() > 1 && mapName[0] == 'e' ? mapName[1] - '0' : 1;
        auto skyName =
            std::string("SKY") + std::to_string(episode >= 1 && episode <= 3 ? episode : 1);
        Image sky = image(skyName, false);
        images[skyName] = sky;
        flats[skyName] = false;
        const char *suffix[] = {"rt", "lf", "bk", "ft", "up", "dn"};
        const int axis[6][3] = {{3, -1, 2},  {-3, 1, 2},  {1, 3, 2},
                                {-1, -3, 2}, {-2, -1, 3}, {2, -1, -3}};
        for (int face = 0; face < 6; ++face) {
            Image cube;
            cube.width = cube.height = 128;
            cube.pixels.resize(128 * 128 * 4);
            for (int y = 0; y < 128; ++y)
                for (int x = 0; x < 128; ++x) {
                    double q[] = {2 * (x + 0.5) / 128 - 1, 1 - 2 * (y + 0.5) / 128, 1}, d[3];
                    for (int j = 0; j < 3; ++j) {
                        int n = axis[face][j];
                        d[j] = q[std::abs(n) - 1] * (n < 0 ? -1 : 1);
                    }
                    double angle = std::atan2(d[1], d[0]);
                    double u = angle / (2 * 3.141592653589793);
                    u -= std::floor(u);
                    double v = 0.5 - std::atan2(d[2], std::hypot(d[0], d[1])) / 3.141592653589793;
                    int sx = std::min(sky.width - 1, int(u * sky.width)),
                        sy = std::max(0, std::min(sky.height - 1, int(v * sky.height)));
                    std::copy_n(sky.pixels.data() + (size_t(sy) * sky.width + sx) * 4, 4,
                                cube.pixels.data() + (size_t(y) * 128 + x) * 4);
                }
            writePng(base / "textures" / mapName / (std::string("_sky_") + suffix[face] + ".png"),
                     cube);
        }
    }
    fs::create_directories(base / "scripts");
    std::ostringstream shader;
    shader << "// Generated by doommap. Source: Doom WAD PLAYPAL/PNAMES/TEXTURE1/TEXTURE2.\n// "
              "Sector brightness is vertex color; no lightmaps.\n";
    for (auto &entry : images) {
        auto &name = entry.first;
        auto &im = entry.second;
        writePng(base / "textures" / mapName / (lower(name) + ".png"), im);
        std::string path = "textures/" + mapName + "/" + lower(name);
        shader << path << "\n{\n    qer_editorimage " << path
               << ".png\n    cull none\n    nomipmaps\n    nopicmip\n";
        if (name == "F_SKY1") {
            shader << "    surfaceparm sky\n    surfaceparm noimpact\n    skyparms textures/"
                   << mapName << "/_sky 0 -\n}\n\n";
            continue;
        }
        shader << "    {\n";
        auto original = name;
        if (flats.at(name) && name.size() > 5 && name.substr(name.size() - 5) == "_FLAT")
            original = name.substr(0, name.size() - 5);
        auto frames = wad.animation(original, flats.at(name));
        if (frames.size() > 1) {
            shader << "        animMap 4.375";
            for (auto &frame : frames)
                shader << " textures/" << mapName << "/" << lower(frame) << ".png";
            shader << '\n';
        } else
            shader << "        map " << path << ".png\n";
        if (im.masked)
            shader << "        alphaFunc GE128\n        depthWrite\n";
        shader << "        rgbGen vertex\n    }\n}\n\n";
    }
    for (auto &name : scrolling) {
        auto path = "textures/" + mapName + "/" + lower(name);
        shader << path
               << "_scroll\n{\n    cull none\n    nomipmaps\n    nopicmip\n    {\n        map "
               << path << ".png\n        rgbGen vertex\n        tcMod scroll "
               << -35.0 / images.at(name).width << " 0\n    }\n}\n\n";
    }
    // Truncate the requested map's old shader, without touching unrelated scripts.
    auto text = shader.str();
    writeFile(base / "scripts" / (mapName + ".shader"), Bytes(text.begin(), text.end()));
}
} // namespace dm
