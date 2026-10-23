#include "BlockWorld.h"
#include "EditWorld.h"
#include "Wadfile.h"
#include "idfunctions.h"
#include <cmath>
#include <map>
namespace de {
void EditWorld::importMap(const Wadfile &wad, const std::string &marker) {
    int base = wad.find(marker);
    require(base >= 0, "Map not found");
    auto lump = [&](const std::string &name) -> const Bytes & {
        for (size_t i = base + 1; i < wad.lumps.size() && i <= size_t(base) + 10; ++i)
            if (wad.lumps[i].name == name)
                return wad.lumps[i].data;
        throw std::runtime_error("Map is missing " + name);
    };
    auto &v = lump("VERTEXES");
    auto &l = lump("LINEDEFS");
    auto &s = lump("SIDEDEFS");
    auto &sec = lump("SECTORS");
    auto &th = lump("THINGS");
    require(v.size() % 4 == 0 && l.size() % 14 == 0 && s.size() % 30 == 0 && sec.size() % 26 == 0 &&
                th.size() % 10 == 0,
            "Invalid Doom map record lengths (Hexen/UDMF maps are not supported)");
    std::vector<FloorSlope> slopes(sec.size() / 26);
    std::vector<FloorSlope> ceilings(slopes.size());
    std::vector<SectorRotation> rotations(slopes.size());
    std::vector<TextureUV> planeUVs(slopes.size() * 2), wallUVs(s.size() / 30);
    std::vector<int32_t> thingHeights(th.size() / 10);
    for (size_t i = base + 11; i < wad.lumps.size(); ++i) {
        const auto &extension = wad.lumps[i];
        if (extension.name == "DPORTALS") {
            throw std::runtime_error("Sector-over-sector WADs are not supported");
        }
        if (extension.name == "DTHINGZ") {
            thingHeights = decodeThingHeights(extension.data.data(), extension.data.size(),
                                              thingHeights.size());
            continue;
        }
        if (extension.name == "DSURFUV" || extension.name == "DWALLUV") {
            auto &uvs = extension.name == "DSURFUV" ? planeUVs : wallUVs;
            uvs = decodeTextureUVs(extension.data.data(), extension.data.size(), uvs.size());
            continue;
        }
        if (extension.name == "DSURFROT") {
            rotations =
                decodeRotations(extension.data.data(), extension.data.size(), rotations.size());
            continue;
        }
        if (extension.name != "DSLOPES" && extension.name != "DCSLOPES")
            break;
        auto &planes = extension.name == "DSLOPES" ? slopes : ceilings;
        planes = decodeSlopes(extension.data.data(), extension.data.size(), planes.size());
    }
    EditWorld out;
    for (size_t i = 0; i < v.size(); i += 4)
        out.points.push_back({{float(i16(v, i)), float(i16(v, i + 2))}, 0});
    auto readSide = [&](int n) {
        require(n >= 0 && size_t(n) < s.size() / 30, "Invalid sidedef index");
        size_t q = size_t(n) * 30;
        Side r;
        r.uv = wallUVs[n];
        r.firstcolumn = i16(s, q);
        r.flags = i16(s, q + 2);
        r.toptexture = name8(&s[q + 4]);
        r.bottomtexture = name8(&s[q + 12]);
        r.midtexture = name8(&s[q + 20]);
        r.sector = u16(s, q + 28);
        require(size_t(r.sector) < sec.size() / 26, "Invalid sector index");
        r.ends.slope = slopes[r.sector];
        r.ends.ceilingSlope = ceilings[r.sector];
        r.ends.rotation = rotations[r.sector];
        r.ends.floorUV = planeUVs[r.sector * 2];
        r.ends.ceilingUV = planeUVs[r.sector * 2 + 1];
        q = size_t(r.sector) * 26;
        r.ends.floorheight = i16(sec, q);
        r.ends.ceilingheight = i16(sec, q + 2);
        r.ends.floorflat = name8(&sec[q + 4]);
        r.ends.ceilingflat = name8(&sec[q + 12]);
        r.ends.lightlevel = i16(sec, q + 20);
        r.ends.special = i16(sec, q + 22);
        r.ends.tag = i16(sec, q + 24);
        return r;
    };
    for (size_t i = 0; i < l.size(); i += 14) {
        Line line;
        line.p1 = u16(l, i);
        line.p2 = u16(l, i + 2);
        require(line.p1 < out.points.size() && line.p2 < out.points.size(), "Invalid vertex index");
        line.flags = u16(l, i + 4);
        line.special = u16(l, i + 6);
        line.tag = u16(l, i + 8);
        line.side[0] = readSide(u16(l, i + 10));
        if (line.flags & 4)
            line.side[1] = readSide(u16(l, i + 12));
        out.lines.push_back(line);
    }
    for (size_t i = 0; i < th.size(); i += 10)
        out.things.push_back({{float(i16(th, i)), float(i16(th, i + 2))},
                              i16(th, i + 4),
                              u16(th, i + 6),
                              u16(th, i + 8),
                              0});
    out.explicitSectors = true;
    for (size_t i = 0; i < out.things.size(); ++i)
        out.things[i].height = thingHeights[i];
    out.markSaved();
    *this = std::move(out);
}
static void coord(Bytes &b, float x) {
    require(std::isfinite(x) && x >= -32768 && x <= 32767 && x == std::round(x),
            "Map coordinates must be integers in -32768..32767");
    put16(b, int(x));
}
void EditWorld::exportMap(const std::filesystem::path &path, const std::string &marker) const {
    auto errors = validate(false);
    require(errors.empty(), errors.empty() ? "" : errors.front());
    EditWorld w = *this;
    auto sectors = connectSectors(w);
    require(sectors.size() < 32768, "Too many sectors");
    Wadfile wad;
    wad.add(marker, {});
    Bytes thingsData, vertices, linesData, sidesData, sectorsData;
    std::vector<TextureUV> wallUVs, planeUVs;
    for (auto &t : things)
        if (t.selected >= 0) {
            coord(thingsData, std::round(t.origin.x));
            coord(thingsData, std::round(t.origin.y));
            put16(thingsData, t.angle);
            put16(thingsData, t.type);
            put16(thingsData, t.options);
        }
    wad.add("THINGS", std::move(thingsData));
    std::map<int, int> ids;
    for (auto &l : w.lines)
        if (l.selected >= 0)
            for (int p : {l.p1, l.p2})
                if (!ids.count(p)) {
                    int id = int(ids.size());
                    require(id < 65535, "Too many vertices");
                    ids[p] = id;
                    coord(vertices, points[p].pt.x);
                    coord(vertices, points[p].pt.y);
                }
    for (auto &l : w.lines)
        if (l.selected >= 0) {
            put16(linesData, ids[l.p1]);
            put16(linesData, ids[l.p2]);
            put16(linesData, l.flags);
            put16(linesData, l.special);
            put16(linesData, l.tag);
            int n = int(sidesData.size() / 30);
            require(n + ((l.flags & 4) ? 2 : 1) < 65535, "Too many sidedefs");
            put16(linesData, n);
            put16(linesData, (l.flags & 4) ? n + 1 : 65535);
            for (int i = 0; i < ((l.flags & 4) ? 2 : 1); ++i) {
                auto &s = l.side[i];
                wallUVs.push_back(s.uv);
                put16(sidesData, s.firstcolumn);
                put16(sidesData, s.flags);
                putName(sidesData, s.toptexture);
                putName(sidesData, s.bottomtexture);
                putName(sidesData, s.midtexture);
                put16(sidesData, s.sector);
            }
        }
    for (auto &sec : sectors) {
        auto &s = sec.def;
        planeUVs.push_back(s.floorUV);
        planeUVs.push_back(s.ceilingUV);
        put16(sectorsData, s.floorheight);
        put16(sectorsData, s.ceilingheight);
        putName(sectorsData, s.floorflat);
        putName(sectorsData, s.ceilingflat);
        put16(sectorsData, s.lightlevel);
        put16(sectorsData, s.special);
        put16(sectorsData, s.tag);
    }
    wad.add("LINEDEFS", std::move(linesData));
    wad.add("SIDEDEFS", std::move(sidesData));
    wad.add("VERTEXES", std::move(vertices));
    wad.add("SEGS", {});
    wad.add("SSECTORS", {});
    wad.add("NODES", {});
    wad.add("SECTORS", std::move(sectorsData));
    wad.add("REJECT", {});
    wad.add("BLOCKMAP", {});
    std::vector<FloorSlope> slopes;
    for (const auto &sector : sectors)
        slopes.push_back(sector.def.slope);
    auto slopeData = encodeSlopes(slopes);
    if (!slopeData.empty())
        wad.add("DSLOPES", std::move(slopeData));
    slopes.clear();
    for (const auto &sector : sectors)
        slopes.push_back(sector.def.ceilingSlope);
    slopeData = encodeSlopes(slopes);
    if (!slopeData.empty())
        wad.add("DCSLOPES", std::move(slopeData));
    std::vector<SectorRotation> rotations;
    for (const auto &sector : sectors)
        rotations.push_back(sector.def.rotation);
    auto rotationData = encodeRotations(rotations);
    if (!rotationData.empty())
        wad.add("DSURFROT", std::move(rotationData));
    auto planeData = encodeTextureUVs(planeUVs), wallData = encodeTextureUVs(wallUVs);
    if (!planeData.empty())
        wad.add("DSURFUV", std::move(planeData));
    if (!wallData.empty())
        wad.add("DWALLUV", std::move(wallData));
    std::vector<int32_t> thingHeights;
    for (const auto &t : things)
        if (t.selected >= 0)
            thingHeights.push_back(t.height);
    auto heightData = encodeThingHeights(thingHeights);
    if (!heightData.empty())
        wad.add("DTHINGZ", std::move(heightData));
    wad.save(path);
}
// Original 1993 editor output: counted POINTS/LINES/SECTORS/THINGS and FLATNAME.
void EditWorld::exportLegacy(const std::filesystem::path &path, const std::string &marker,
                             const std::vector<Texture> &textures) const {
    EditWorld w = *this;
    auto sectors = connectSectors(w);
    Wadfile wad;
    for (const auto &t : things)
        if (t.selected >= 0)
            require(t.height == 0,
                    "Thing heights require playable WAD export; the 1993 format cannot store them");
    for (const auto &l : lines)
        if (l.selected >= 0)
            for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                const auto &s = l.side[side];
                require(!s.uv.active() && !s.ends.floorUV.active() && !s.ends.ceilingUV.active(),
                        "Texture UV transforms require playable WAD export; the 1993 format cannot "
                        "store them");
            }
    wad.add(marker, {});
    for (const auto &sector : sectors)
        require(!sector.def.slope.active() && !sector.def.ceilingSlope.active() &&
                    !sector.def.rotation.active(),
                "Slopes require playable WAD export; the 1993 format cannot store them");
    Bytes flats, verts, ld, sd, td;
    std::vector<std::string> names;
    auto flat = [&](const std::string &n) {
        for (size_t i = 0; i < names.size(); ++i)
            if (names[i] == n)
                return int(i);
        names.push_back(n);
        return int(names.size() - 1);
    };
    auto tex = [&](const std::string &n) {
        if (n == "-" || n.empty())
            return -1;
        for (size_t i = 0; i < textures.size(); ++i)
            if (textures[i].name == n)
                return int(i);
        throw std::runtime_error("Unknown texture " + n);
    };
    std::map<int, int> ids, lineids;
    for (size_t i = 0; i < w.lines.size(); ++i)
        if (w.lines[i].selected >= 0) {
            int index = int(lineids.size());
            lineids[int(i)] = index;
            for (int p : {w.lines[i].p1, w.lines[i].p2})
                if (!ids.count(p)) {
                    int v = int(ids.size());
                    ids[p] = v;
                }
        }
    put32(verts, uint32_t(ids.size()));
    std::vector<int> order(ids.size());
    for (auto [p, i] : ids)
        order[i] = p;
    for (int p : order) {
        coord(verts, w.points[p].pt.x);
        coord(verts, w.points[p].pt.y);
    }
    put32(ld, uint32_t(lineids.size()));
    for (auto &l : w.lines)
        if (l.selected >= 0) {
            put16(ld, ids[l.p1]);
            put16(ld, ids[l.p2]);
            put16(ld, l.flags);
            auto d = w.points[l.p2].pt - w.points[l.p1].pt;
            put16(ld, int(std::hypot(d.x, d.y)));
            put16(ld, l.special);
            put16(ld, l.tag);
            for (int i = 0; i < 2; ++i) {
                if (i == 1 && !(l.flags & 4)) {
                    ld.resize(ld.size() + 12);
                    continue;
                }
                auto &s = l.side[i];
                put16(ld, s.flags);
                put16(ld, s.sector);
                put16(ld, s.firstcolumn);
                put16(ld, tex(s.midtexture));
                put16(ld, tex(s.toptexture));
                put16(ld, tex(s.bottomtexture));
            }
        }
    put32(sd, uint32_t(sectors.size()));
    sd.resize(4 + 4 * sectors.size());
    for (size_t i = 0; i < sectors.size(); ++i) {
        uint32_t offset = uint32_t(sd.size());
        for (int j = 0; j < 4; ++j)
            sd[4 + 4 * i + j] = uint8_t(offset >> (j * 8));
        auto &s = sectors[i].def;
        put16(sd, s.floorheight);
        put16(sd, s.ceilingheight);
        put16(sd, flat(s.floorflat));
        put16(sd, flat(s.ceilingflat));
        put16(sd, s.lightlevel);
        put16(sd, s.special);
        put16(sd, s.tag);
        put16(sd, int(sectors[i].sides.size()));
        for (auto [line, side] : sectors[i].sides)
            put16(sd, lineids[line]);
    }
    int count = 0;
    for (auto &t : things)
        if (t.selected >= 0)
            ++count;
    put32(td, count);
    for (auto &t : things)
        if (t.selected >= 0) {
            int side = 0, line = lineByPoint(w, t.origin, side);
            require(line >= 0 && (side == 0 || (w.lines[line].flags & 4)), "Thing outside map");
            coord(td, std::round(t.origin.x));
            coord(td, std::round(t.origin.y));
            put16(td, t.angle);
            put16(td, t.type);
            put16(td, t.options);
            put16(td, w.lines[line].side[side].sector);
        }
    put32(flats, uint32_t(names.size()));
    for (auto &n : names)
        putName(flats, n);
    wad.add("FLATNAME", flats);
    wad.add("POINTS", verts);
    wad.add("LINES", ld);
    wad.add("SECTORS", sd);
    wad.add("THINGS", td);
    wad.save(path);
}
} // namespace de
