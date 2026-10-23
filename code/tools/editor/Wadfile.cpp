#include "Wadfile.h"
#include "idfunctions.h"
#include <algorithm>
#include <ostream>
namespace de {
void Wadfile::load(const std::filesystem::path &p) {
    auto b = readFile(p);
    require(b.size() >= 12, "Truncated WAD header");
    require(std::string(b.begin(), b.begin() + 4) == "IWAD" ||
                std::string(b.begin(), b.begin() + 4) == "PWAD",
            "Not an IWAD or PWAD");
    size_t n = u32(b, 4), off = u32(b, 8);
    require(n <= 1000000 && off <= b.size() && n <= (b.size() - off) / 16, "Invalid WAD directory");
    std::vector<Lump> result;
    for (size_t i = 0; i < n; ++i) {
        size_t q = off + i * 16, pos = u32(b, q), size = u32(b, q + 4);
        require(pos <= b.size() && size <= b.size() - pos, "WAD lump extends beyond file");
        result.push_back({name8(&b[q + 8]), Bytes(b.begin() + pos, b.begin() + pos + size)});
    }
    lumps = std::move(result);
}
static void saveLumps(const std::filesystem::path &p, const std::vector<const Lump *> &lumps) {
    require(lumps.size() <= 1000000, "Too many WAD lumps");
    uint64_t size = 12;
    std::vector<uint32_t> offsets;
    offsets.reserve(lumps.size());
    for (const auto *l : lumps) {
        require(l->name.size() <= 8, "Lump name too long");
        offsets.push_back(uint32_t(size));
        size += l->data.size();
        require(size < 0x7fffffff, "WAD too large");
    }
    require(size + lumps.size() * 16 < 0x7fffffff, "WAD directory too large");
    Bytes b = {'P', 'W', 'A', 'D'};
    put32(b, uint32_t(lumps.size()));
    put32(b, uint32_t(size));
    // Stream existing lump storage directly. No full-WAD allocation or
    // growing byte vector competes with the editor's loaded 3D scene.
    writeFileAtomic(p, [&](std::ostream &f) {
        f.write(reinterpret_cast<const char *>(b.data()), b.size());
        for (const auto *l : lumps)
            if (!l->data.empty())
                f.write(reinterpret_cast<const char *>(l->data.data()), l->data.size());
        Bytes entry;
        entry.reserve(16);
        for (size_t i = 0; i < lumps.size(); ++i) {
            entry.clear();
            put32(entry, offsets[i]);
            put32(entry, uint32_t(lumps[i]->data.size()));
            putName(entry, lumps[i]->name);
            f.write(reinterpret_cast<const char *>(entry.data()), entry.size());
        }
    });
}
void Wadfile::save(const std::filesystem::path &p) const {
    std::vector<const Lump *> records;
    records.reserve(lumps.size());
    for (const auto &l : lumps)
        records.push_back(&l);
    saveLumps(p, records);
}
void Wadfile::saveWithMaterials(const std::filesystem::path &p, const Wadfile &resources) const {
    const auto definitions = resources.materials();
    if (definitions.empty()) {
        save(p);
        return;
    }
    require(find("DMATDEF") < 0, "Destination already contains a material manifest");
    const auto manifest = doom_materials::serialize(definitions);
    const Lump metadata{"DMATDEF", Bytes(manifest.begin(), manifest.end())};
    std::vector<const Lump *> records;
    records.reserve(lumps.size() + definitions.size() + 1);
    for (const auto &l : lumps)
        records.push_back(&l);
    records.push_back(&metadata);
    for (const auto &d : definitions) {
        require(find(d.name) < 0, "Material collides with map lump: " + d.name);
        const auto index = resources.find(d.name);
        require(index >= 0, "Missing WAD lump: " + d.name);
        records.push_back(&resources.lumps[index]);
    }
    saveLumps(p, records);
}
int Wadfile::find(const std::string &n) const {
    auto key = upper(n);
    for (size_t i = lumps.size(); i > 0; --i)
        if (lumps[i - 1].name == key)
            return int(i - 1);
    return -1;
}
const Bytes &Wadfile::get(const std::string &n) const {
    int i = find(n);
    require(i >= 0, "Missing WAD lump: " + n);
    return lumps[i].data;
}
void Wadfile::add(const std::string &n, Bytes b) {
    require(n.size() <= 8, "Lump name too long");
    lumps.push_back({upper(n), std::move(b)});
}
std::vector<std::string> Wadfile::mapNames() const {
    std::vector<std::string> r;
    for (size_t i = 0; i + 1 < lumps.size(); ++i)
        if (lumps[i + 1].name == "THINGS")
            r.push_back(lumps[i].name);
    return r;
}
std::vector<std::string> Wadfile::flatNames(bool hdOnly) const {
    std::vector<std::string> r;
    if (!mapImages.empty()) {
        for (const auto &entry : mapImages) {
            r.push_back(entry.first);
        }
        return r;
    }
    bool active = false;
    for (auto &l : lumps) {
        if (l.name == "F_START" || l.name == "FF_START")
            active = true;
        else if (l.name == "F_END" || l.name == "FF_END")
            active = false;
        else if (!hdOnly && active && l.data.size() == 4096)
            r.push_back(l.name);
    }
    for (const auto &d : materials())
        if ((d.kind == 'F' || d.kind == 'S'))
            r.push_back(d.name);
    return r;
}
std::vector<std::string> Wadfile::patchNames() const {
    std::vector<std::string> r;
    if (find("PNAMES") < 0)
        return r;
    auto &b = get("PNAMES");
    size_t n = u32(b, 0);
    require(n <= (b.size() - 4) / 8, "Invalid PNAMES");
    for (size_t i = 0; i < n; ++i)
        r.push_back(name8(&b[4 + i * 8]));
    return r;
}
std::vector<Texture> Wadfile::textures(bool mapCatalog) const {
    std::vector<Texture> r;
    if (mapCatalog && !mapImages.empty()) {
        for (const auto &entry : mapImages) {
            const auto bytes = readFile(entry.second);
            require(bytes.size() >= 24 && memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0,
                    "Invalid PNG header: " + entry.second.u8string());
            auto be32 = [&](size_t offset) {
                return (uint32_t(bytes[offset]) << 24) | (uint32_t(bytes[offset + 1]) << 16) |
                       (uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
            };
            const auto width = be32(16), height = be32(20);
            require(width && height && width <= 4096 && height <= 4096,
                    "PNG dimensions must be 1..4096: " + entry.second.u8string());
            Texture texture;
            texture.name = entry.first;
            texture.width = static_cast<int>(width);
            texture.height = static_cast<int>(height);
            texture.wadIndex = 2;
            r.push_back(std::move(texture));
        }
        return r;
    }
    auto names = patchNames();
    for (auto label : {"TEXTURE1", "TEXTURE2"}) {
        if (find(label) < 0)
            continue;
        auto &b = get(label);
        size_t n = u32(b, 0);
        require(n <= (b.size() - 4) / 4, "Invalid texture directory");
        for (size_t i = 0; i < n; ++i) {
            size_t off = u32(b, 4 + i * 4);
            require(off <= b.size() && b.size() - off >= 22, "Invalid texture offset");
            Texture t;
            t.wadIndex = label[7] - '1';
            t.name = name8(&b[off]);
            t.width = u16(b, off + 12);
            t.height = u16(b, off + 14);
            size_t count = u16(b, off + 20);
            require(count <= (b.size() - off - 22) / 10, "Invalid texture patches");
            for (size_t j = 0; j < count; ++j) {
                size_t q = off + 22 + j * 10;
                int idx = u16(b, q + 4);
                require(idx < int(names.size()), "Invalid patch index");
                t.patches.push_back(
                    {i16(b, q), i16(b, q + 2), i16(b, q + 6), i16(b, q + 8), names[idx]});
            }
            r.push_back(t);
        }
    }
    for (const auto &d : materials()) {
        if (d.kind != 'W')
            continue;
        Texture t;
        t.name = d.name;
        t.width = d.width;
        t.height = d.height;
        t.wadIndex = 2; // PNG material, not a classic patch composition.
        r.push_back(t);
    }
    return r;
}
std::array<uint32_t, 256> Wadfile::palette() const {
    std::array<uint32_t, 256> p{};
    if (find("PLAYPAL") < 0) {
        for (int i = 0; i < 256; ++i)
            p[i] = 0xff000000u | uint32_t(i) * 0x010101;
        return p;
    }
    auto &b = get("PLAYPAL");
    require(b.size() >= 768, "Truncated PLAYPAL");
    for (int i = 0; i < 256; ++i)
        p[i] =
            0xff000000u | b[3 * i] | (uint32_t(b[3 * i + 1]) << 8) | (uint32_t(b[3 * i + 2]) << 16);
    return p;
}
Image Wadfile::flat(const std::string &n) const {
    if (mapImages.count(upper(n))) {
        return materialImage(n);
    }
    for (const auto &texture : textures(false)) {
        if (texture.name == upper(n) && find(n) < 0) {
            return this->texture(texture);
        }
    }
    for (const auto &d : materials())
        if ((d.kind == 'F' || d.kind == 'S') && d.name == upper(n))
            return materialImage(d.name);
    auto &b = get(n);
    require(b.size() == 4096, "Flat must be 64 x 64");
    Image im{64, 64, {}};
    auto p = palette();
    for (auto v : b)
        im.pixels.push_back(p[v]);
    return im;
}
Image Wadfile::patch(const std::string &n) const {
    auto &b = get(n);
    require(b.size() >= 8, "Truncated patch");
    int w = u16(b, 0), h = u16(b, 2);
    require(w > 0 && h > 0 && w <= 4096 && h <= 4096 && size_t(w) <= (b.size() - 8) / 4,
            "Invalid patch dimensions");
    Image im{w, h, std::vector<uint32_t>(size_t(w) * h)};
    auto pal = palette();
    for (int x = 0; x < w; ++x) {
        size_t q = u32(b, 8 + 4 * x);
        int previous = -1;
        for (;;) {
            require(q < b.size(), "Truncated patch column");
            int top = b[q++];
            if (top == 255)
                break;
            require(q + 2 <= b.size(), "Truncated patch post");
            int len = b[q++];
            ++q;
            require(size_t(len) + 1 <= b.size() - q, "Truncated patch pixels");
            if (top <= previous)
                top += previous;
            previous = top;
            for (int y = 0; y < len; ++y)
                if (top + y < h)
                    im.pixels[size_t(top + y) * w + x] = pal[b[q + y]];
            q += len + 1;
        }
    }
    return im;
}
Image Wadfile::texture(const Texture &t) const {
    if (t.wadIndex == 3) {
        return flat(t.name);
    }
    if (mapImages.count(upper(t.name))) {
        return materialImage(t.name);
    }
    if (t.wadIndex == 2)
        return materialImage(t.name);
    require(t.width > 0 && t.height > 0 && t.width <= 4096 && t.height <= 4096,
            "Invalid texture dimensions");
    Image out{t.width, t.height, std::vector<uint32_t>(size_t(t.width) * t.height)};
    for (auto &p : t.patches) {
        auto im = patch(p.name);
        for (int y = 0; y < im.height; ++y)
            for (int x = 0; x < im.width; ++x) {
                int dx = x + p.x, dy = y + p.y;
                if (dx >= 0 && dx < t.width && dy >= 0 && dy < t.height) {
                    auto color = im.pixels[size_t(y) * im.width + x];
                    if (color >> 24)
                        out.pixels[size_t(dy) * t.width + dx] = color;
                }
            }
    }
    return out;
}
} // namespace de
