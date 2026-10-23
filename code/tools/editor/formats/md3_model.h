#pragma once
// MD3 v15 reader. Layout reference: ioquake3 code/qcommon/qfiles.h.
// Shared by Doom and DoomEd; all disk offsets and allocation sizes are checked.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace doom_models {
struct Vec3 {
    float x = 0, y = 0, z = 0;
};
struct Vertex {
    Vec3 position, normal;
};
struct Surface {
    std::string name, shader;
    std::vector<std::array<unsigned, 3>> triangles;
    std::vector<std::array<float, 2>> uv;
    std::vector<std::vector<Vertex>> frames;
    bool fullbright() const { return name.rfind("glow_", 0) == 0; }
};
struct Tag {
    std::string name;
    Vec3 origin;
    std::array<Vec3, 3> axis;
};
struct Model {
    unsigned frames = 0;
    std::vector<Surface> surfaces;
    std::vector<std::vector<Tag>> tags;
};
inline void require(bool ok, const std::string &message) {
    if (!ok)
        throw std::runtime_error("MD3: " + message);
}
inline std::vector<uint8_t> read(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    require(bool(in), "cannot open " + path.u8string());
    auto size = in.tellg();
    require(size >= 0 && size <= 128 * 1024 * 1024, "file too large");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    in.seekg(0);
    in.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
    require(bool(in), "cannot read " + path.u8string());
    return bytes;
}
inline Model decode(const std::vector<uint8_t> &b) {
    auto range = [&](size_t o, size_t n, size_t end) {
        require(o <= end && n <= end - o && end <= b.size(), "invalid/truncated range");
    };
    auto u32 = [&](size_t o) {
        range(o, 4, b.size());
        return uint32_t(b[o]) | uint32_t(b[o + 1]) << 8 | uint32_t(b[o + 2]) << 16 |
               uint32_t(b[o + 3]) << 24;
    };
    auto f32 = [&](size_t o) {
        auto bits = u32(o);
        float f;
        std::memcpy(&f, &bits, 4);
        require(std::isfinite(f), "non-finite number");
        return f;
    };
    auto str = [&](size_t o, size_t n) {
        range(o, n, b.size());
        size_t k = 0;
        while (k < n && b[o + k])
            ++k;
        require(k < n, "unterminated name");
        return std::string(reinterpret_cast<const char *>(b.data() + o), k);
    };
    auto vec = [&](size_t o) { return Vec3{f32(o), f32(o + 4), f32(o + 8)}; };
    range(0, 108, b.size());
    require(u32(0) == 0x33504449 && u32(4) == 15, "expected IDP3 version 15");
    auto nf = u32(76), nt = u32(80), ns = u32(84), of = u32(92), ot = u32(96), os = u32(100),
         end = u32(104);
    require(nf >= 1 && nf <= 1024 && nt <= 16 && ns >= 1 && ns <= 32, "invalid model counts");
    require(end == b.size() && of >= 108 && ot >= of + nf * 56 && os >= ot + nf * nt * 112,
            "invalid header offsets");
    range(of, size_t(nf) * 56, end);
    range(ot, size_t(nf) * nt * 112, end);
    for (unsigned f = 0; f < nf; ++f)
        for (unsigned n = 0; n < 10; ++n)
            f32(of + f * 56 + n * 4);
    Model result;
    result.frames = nf;
    result.tags.resize(nf);
    for (unsigned f = 0; f < nf; ++f)
        for (unsigned t = 0; t < nt; ++t) {
            size_t o = ot + (size_t(f) * nt + t) * 112;
            result.tags[f].push_back(
                {str(o, 64), vec(o + 64), {vec(o + 76), vec(o + 88), vec(o + 100)}});
        }
    size_t base = os, totalVertices = 0;
    for (unsigned s = 0; s < ns; ++s) {
        range(base, 108, end);
        require(u32(base) == 0x33504449, "invalid surface signature");
        auto sf = u32(base + 72), sh = u32(base + 76), nv = u32(base + 80), tri = u32(base + 84);
        auto ti = u32(base + 88), sk = u32(base + 92), st = u32(base + 96), xyz = u32(base + 100),
             se = u32(base + 104);
        require(sf == nf && sh >= 1 && sh <= 256 && nv >= 3 && nv <= 4096 && tri >= 1 &&
                    tri <= 8192,
                "invalid surface counts");
        totalVertices += size_t(nv) * nf;
        require(totalVertices <= 4 * 1024 * 1024, "model animation exceeds memory limit");
        range(base, se, end);
        require(se >= 108, "invalid surface size");
        std::array<std::pair<size_t, size_t>, 4> chunks{{{ti, size_t(tri) * 12},
                                                         {sk, size_t(sh) * 68},
                                                         {st, size_t(nv) * 8},
                                                         {xyz, size_t(nv) * nf * 8}}};
        std::sort(chunks.begin(), chunks.end());
        size_t previous = 108;
        for (auto chunk : chunks) {
            require(chunk.first >= previous, "overlapping surface data");
            range(base + chunk.first, chunk.second, base + se);
            previous = chunk.first + chunk.second;
        }
        Surface surf;
        surf.name = str(base + 4, 64);
        surf.shader = str(base + sk, 64);
        for (unsigned i = 0; i < tri; ++i) {
            std::array<unsigned, 3> indices{};
            for (unsigned j = 0; j < 3; ++j) {
                indices[j] = u32(base + ti + i * 12 + j * 4);
                require(indices[j] < nv, "triangle index out of range");
            }
            surf.triangles.push_back(indices);
        }
        for (unsigned i = 0; i < nv; ++i) {
            float u = f32(base + st + i * 8), v = f32(base + st + i * 8 + 4);
            require(std::abs(u) <= 65536 && std::abs(v) <= 65536, "UV out of range");
            surf.uv.push_back({u, v});
        }
        surf.frames.resize(nf);
        for (unsigned f = 0; f < nf; ++f)
            for (unsigned i = 0; i < nv; ++i) {
                size_t o = base + xyz + (size_t(f) * nv + i) * 8;
                auto coord = [&](size_t p) {
                    return float(int16_t(uint16_t(b[p]) | uint16_t(b[p + 1]) << 8)) / 64.f;
                };
                float lat = b[o + 7] * (6.28318530718f / 255),
                      lng = b[o + 6] * (6.28318530718f / 255);
                surf.frames[f].push_back({{coord(o), coord(o + 2), coord(o + 4)},
                                          {std::cos(lat) * std::sin(lng),
                                           std::sin(lat) * std::sin(lng), std::cos(lng)}});
            }
        result.surfaces.push_back(std::move(surf));
        base += se;
    }
    require(base == end, "surface chain does not end at file boundary");
    return result;
}
inline std::filesystem::path relativeAsset(const std::filesystem::path &directory,
                                           const std::string &name) {
    auto path = std::filesystem::u8path(name);
    require(!name.empty() && !path.is_absolute() && !path.has_root_name(),
            "asset path must be relative");
    for (const auto &part : path)
        require(part != "..", "asset path escapes model directory");
    return directory / path;
}
struct Definition {
    int type = 0;
    std::string name;
    std::filesystem::path file;
    float scale = 1, radius = 8, height = 64;
    int first = 0, last = 0;
    float fps = 0;
};
struct Registry {
    std::map<int, Definition> definitions;
    std::map<int, Model> models;
    void discover(const std::filesystem::path &root) {
        definitions.clear();
        models.clear();
        if (!std::filesystem::exists(root))
            return;
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
            if (entry.is_regular_file() && entry.path().filename() == "models.txt")
                files.push_back(entry.path());
        std::sort(files.begin(), files.end());
        for (const auto &file : files) {
            std::ifstream in(file);
            std::string line;
            std::getline(in, line);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            require(line == "DOOM_MODELS 1", "invalid manifest " + file.u8string());
            while (std::getline(in, line)) {
                if (line.empty() || line.find_first_not_of(" \t\r") == std::string::npos ||
                    line[line.find_first_not_of(" \t")] == '#')
                    continue;
                Definition d;
                std::string model, extra;
                std::istringstream row(line);
                require(bool(row >> d.type >> std::quoted(d.name) >> std::quoted(model) >>
                             d.scale >> d.radius >> d.height >> d.first >> d.last >> d.fps),
                        "invalid manifest row: " + line);
                require(!(row >> extra), "extra manifest fields");
                require(d.type >= 15000 && d.type <= 30000 && !d.name.empty() &&
                            d.name.size() <= 128 && std::isfinite(d.scale) && d.scale > 0 &&
                            d.scale <= 128 && std::isfinite(d.radius) && d.radius >= 0 &&
                            d.radius <= 128 && std::isfinite(d.height) && d.height > 0 &&
                            d.height <= 4096 && d.first >= 0 && d.last >= d.first &&
                            d.last < 1024 && std::isfinite(d.fps) && d.fps >= 0 && d.fps <= 120,
                        "invalid definition values");
                d.file = relativeAsset(file.parent_path(), model);
                require(definitions.emplace(d.type, d).second,
                        "duplicate thing type " + std::to_string(d.type));
                require(definitions.size() <= 1024, "too many models");
            }
        }
    }
    const Definition *find(int type) const {
        auto i = definitions.find(type);
        return i == definitions.end() ? nullptr : &i->second;
    }
    const Model &load(int type) {
        auto found = models.find(type);
        if (found != models.end())
            return found->second;
        const auto *d = find(type);
        require(d != nullptr, "unknown thing type");
        auto model = decode(read(d->file));
        require(unsigned(d->last) < model.frames, "animation exceeds model frames");
        for (const auto &surface : model.surfaces) {
            auto skin = relativeAsset(d->file.parent_path(), surface.shader);
            require(skin.extension() == ".png" && std::filesystem::is_regular_file(skin),
                    "missing PNG skin " + skin.u8string());
        }
        return models.emplace(type, std::move(model)).first->second;
    }
};
inline Vertex sample(const Surface &s, const Definition &d, unsigned index, double seconds) {
    const int count = d.last - d.first + 1;
    double t = std::fmod(std::max(0., seconds) * d.fps, count);
    int a = d.first + int(t), b = d.first + (int(t) + 1) % count;
    float f = float(t - int(t));
    auto lerp = [&](Vec3 x, Vec3 y) {
        return Vec3{x.x + (y.x - x.x) * f, x.y + (y.y - x.y) * f, x.z + (y.z - x.z) * f};
    };
    const auto &v = s.frames[a][index], &w = s.frames[b][index];
    return {lerp(v.position, w.position), lerp(v.normal, w.normal)};
}
inline Vec3 rotate(Vec3 v, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}
inline float shade(Vec3 normal) {
    const float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    return len < .001f
               ? 1.f
               : .7f +
                     .3f * std::max(0.f,
                                    (normal.x * .3f - normal.y * .4f + normal.z * .8660254f) / len);
}
} // namespace doom_models
