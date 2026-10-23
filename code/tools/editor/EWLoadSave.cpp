#include "EditWorld.h"
#include "idfunctions.h"
#include "ThingSpline.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <set>
namespace de {
std::string EditWorld::serializeForCompiler() const {
    auto snapshot = *this;
    // Editor-only ownership/reference metadata stays in the saved DWD. DoomMap consumes
    // the baked thing instances and reconstructs sectors from the boundary geometry.
    snapshot.explicitSectors = false;
    snapshot.reference = {};
    snapshot.thingSplines.clear();
    for (auto &thing : snapshot.things) {
        require(thing.height == 0, "DoomMap does not support thing height offsets yet");
        thing.splineId = 0;
        thing.origin.x = std::round(thing.origin.x);
        thing.origin.y = std::round(thing.origin.y);
    }
    for (const auto &line : snapshot.lines) {
        if (line.selected < 0) {
            continue;
        }
        for (int sideIndex = 0; sideIndex < ((line.flags & 4) ? 2 : 1); ++sideIndex) {
            const auto &side = line.side[sideIndex];
            const auto &sector = side.ends;
            require(!sector.slope.active() && !sector.ceilingSlope.active(),
                    "DoomMap does not support sloped surfaces yet; the DWD retains these edits");
            require(sector.rotation == SectorRotation{} && side.uv == TextureUV{} &&
                        sector.floorUV == TextureUV{} && sector.ceilingUV == TextureUV{},
                    "DoomMap does not support extended texture UV transforms yet; "
                    "the DWD retains these edits");
        }
    }
    return snapshot.serialize();
}
std::string EditWorld::serialize() const {
    std::ostringstream o;
    o << std::setprecision(9);
    o << "WorldServer version 4\n\nlines:";
    int n = 0;
    for (auto &l : lines)
        if (l.selected >= 0)
            ++n;
    o << n << '\n';
    auto name = [](const std::string &s) { return s.empty() ? std::string("-") : s; };
    for (auto &l : lines)
        if (l.selected >= 0) {
            auto a = points.at(l.p1).pt, b = points.at(l.p2).pt;
            o << '(' << a.x << ',' << a.y << ") to (" << b.x << ',' << b.y << ") : " << l.flags
              << " : " << l.special << " : " << l.tag << '\n';
            for (int i = 0; i < ((l.flags & 4) ? 2 : 1); ++i) {
                auto &s = l.side[i];
                auto &e = s.ends;
                o << "    " << s.flags << " (" << s.firstcolumn << " : " << name(s.toptexture)
                  << " / " << name(s.bottomtexture) << " / " << name(s.midtexture) << " )\n";
                o << "    " << e.floorheight << " : " << name(e.floorflat) << ' ' << e.ceilingheight
                  << " : " << name(e.ceilingflat) << ' ' << e.lightlevel << ' ' << e.special << ' '
                  << e.tag << '\n';
            }
        }
    n = 0;
    for (auto &t : things)
        if (t.selected >= 0)
            ++n;
    o << "\nthings:" << n << '\n';
    for (auto &t : things)
        if (t.selected >= 0)
            o << '(' << t.origin.x << ',' << t.origin.y << ", " << t.angle << ") :" << t.type
              << ", " << t.options << '\n';
    if (std::any_of(things.begin(), things.end(),
                    [](const Thing &t) { return t.selected >= 0 && t.height; })) {
        o << "\n// DoomEd thing heights v1\n" << n << '\n';
        for (const auto &t : things)
            if (t.selected >= 0) {
                require(t.height >= -32768 && t.height <= 32767, "Thing height exceeds map bounds");
                o << t.height << '\n';
            }
    }
    if (explicitSectors) {
        o << "\n// DoomEd sector indices v1\n";
        for (auto &l : lines)
            if (l.selected >= 0)
                o << l.side[0].sector << " " << l.side[1].sector << "\n";
    }
    bool slopes = false;
    for (const auto &l : lines)
        if (l.selected >= 0)
            for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side)
                slopes |= l.side[side].ends.slope.active();
    if (slopes) {
        o << "\n// DoomEd floor slopes v1\n";
        for (const auto &l : lines)
            if (l.selected >= 0)
                for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                    const auto &s = l.side[side].ends.slope;
                    o << s.x << ' ' << s.y << ' ' << s.dx << ' ' << s.dy << '\n';
                }
    }
    bool ceilingSlopes = false;
    for (const auto &l : lines)
        if (l.selected >= 0)
            for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side)
                ceilingSlopes |= l.side[side].ends.ceilingSlope.active();
    if (ceilingSlopes) {
        o << "\n// DoomEd ceiling slopes v1\n";
        for (const auto &l : lines)
            if (l.selected >= 0)
                for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                    const auto &s = l.side[side].ends.ceilingSlope;
                    o << s.x << ' ' << s.y << ' ' << s.dx << ' ' << s.dy << '\n';
                }
    }
    bool rotations = false;
    for (const auto &l : lines)
        if (l.selected >= 0)
            for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side)
                rotations |= l.side[side].ends.rotation.active();
    if (rotations) {
        o << "\n// DoomEd texture rotations v1\n";
        for (const auto &l : lines)
            if (l.selected >= 0)
                for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                    const auto &r = l.side[side].ends.rotation;
                    o << r.floor << ' ' << r.ceiling << '\n';
                }
    }
    bool transforms = false;
    for (const auto &l : lines)
        if (l.selected >= 0)
            for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                const auto &s = l.side[side];
                transforms |= s.uv.active() || s.ends.floorUV.active() || s.ends.ceilingUV.active();
            }
    if (transforms) {
        o << "\n// DoomEd texture UVs v1\n";
        for (const auto &l : lines)
            if (l.selected >= 0)
                for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                    const auto &s = l.side[side];
                    for (const auto &uv : {s.uv, s.ends.floorUV, s.ends.ceilingUV}) {
                        uv.validate();
                        o << uv.scaleU << ' ' << uv.scaleV << ' ' << uv.offsetU << ' ' << uv.offsetV
                          << '\n';
                    }
                }
    }
    if (!thingSplines.empty()) {
        o << "\n// DoomEd thing splines v1\n" << thingSplines.size() << '\n';
        for (const auto &s : thingSplines) {
            validateThingSpline(s);
            o << s.id << ' ' << s.type << ' ' << s.options << ' ' << s.spacing << ' '
              << s.angleOffset << ' ' << s.smooth << ' ' << s.closed << ' ' << s.nodes.size()
              << '\n';
            for (auto p : s.nodes)
                o << p.x << ' ' << p.y << '\n';
        }
        o << "// DoomEd spline thing owners v1\n" << n << '\n';
        for (const auto &t : things)
            if (t.selected >= 0)
                o << t.splineId << '\n';
    }
    if (!reference.path.empty()) {
        auto path = std::filesystem::u8path(reference.path);
        if (path.is_absolute() && !pathname.empty()) {
            std::error_code ec;
            auto relative = std::filesystem::relative(
                path, std::filesystem::absolute(pathname).parent_path(), ec);
            if (!ec && !relative.empty())
                path = relative;
        }
        o << "\n// DoomEd reference image v1\n"
          << std::quoted(path.generic_u8string()) << '\n'
          << reference.center.x << ' ' << reference.center.y << ' ' << reference.scale << ' '
          << reference.opacity << ' ' << reference.rotation << '\n';
    }
    return o.str();
}
void EditWorld::parse(const std::string &text) {
    std::istringstream in(text);
    std::string row;
    auto next = [&]() {
        while (std::getline(in, row)) {
            if (row.find_first_not_of(" \r\t") != std::string::npos)
                return;
        }
        throw std::runtime_error("Unexpected end of map");
    };
    next();
    int version = -1;
    require(sscanf(row.c_str(), "WorldServer version %d", &version) == 1 &&
                (version == 4 || version == 0),
            "Expected WorldServer version 4");
    EditWorld w;
    if (version == 0) {
        clear();
        return;
    }
    next();
    int count = -1;
    require(sscanf(row.c_str(), "lines:%d", &count) == 1 && count >= 0 && count <= 1000000,
            "Invalid line count");
    for (int i = 0; i < count; ++i) {
        next();
        Vec2 a, b;
        Line l;
        require(sscanf(row.c_str(), "(%f,%f) to (%f,%f) : %d : %d : %d", &a.x, &a.y, &b.x, &b.y,
                       &l.flags, &l.special, &l.tag) == 7,
                "Malformed line " + std::to_string(i));
        for (int s = 0; s < ((l.flags & 4) ? 2 : 1); ++s) {
            next();
            auto &side = l.side[s];
            char top[12]{}, bottom[12]{}, mid[12]{};
            require(sscanf(row.c_str(), " %d (%d : %11s / %11s / %11s )", &side.flags,
                           &side.firstcolumn, top, bottom, mid) == 5,
                    "Malformed side");
            side.toptexture = top;
            side.bottomtexture = bottom;
            side.midtexture = mid;
            next();
            auto &e = side.ends;
            char floor[12]{}, ceiling[12]{};
            require(sscanf(row.c_str(), " %d : %11s %d : %11s %d %d %d", &e.floorheight, floor,
                           &e.ceilingheight, ceiling, &e.lightlevel, &e.special, &e.tag) == 7,
                    "Malformed sector");
            e.floorflat = floor;
            e.ceilingflat = ceiling;
        }
        w.newLine(a, b, l);
    }
    next();
    require(sscanf(row.c_str(), "things:%d", &count) == 1 && count >= 0 && count <= 1000000,
            "Invalid thing count");
    for (int i = 0; i < count; ++i) {
        next();
        Thing t;
        require(sscanf(row.c_str(), "(%f,%f, %d) :%d, %d", &t.origin.x, &t.origin.y, &t.angle,
                       &t.type, &t.options) == 5 &&
                    std::isfinite(t.origin.x) && std::isfinite(t.origin.y),
                "Malformed thing");
        w.things.push_back(t);
    }
    bool ownersRead = false, heightsRead = false;
    while (std::getline(in, row)) {
        if (!row.empty() && row.back() == '\r')
            row.pop_back();
        if (row == "// DoomEd thing heights v1") {
            require(!heightsRead, "Duplicate thing height section");
            heightsRead = true;
            next();
            int owners = 0;
            require(sscanf(row.c_str(), "%d", &owners) == 1 && owners == int(w.things.size()),
                    "Invalid thing height count");
            for (auto &t : w.things) {
                next();
                require(sscanf(row.c_str(), "%d", &t.height) == 1 && t.height >= -32768 &&
                            t.height <= 32767,
                        "Invalid thing height");
            }
        } else if (row == "// DoomEd thing splines v1") {
            require(w.thingSplines.empty(), "Duplicate spline section");
            next();
            int splines = 0;
            require(sscanf(row.c_str(), "%d", &splines) == 1 && splines > 0 && splines <= 4096,
                    "Invalid spline count");
            std::set<int> ids;
            for (int i = 0; i < splines; ++i) {
                next();
                ThingSpline s;
                int smooth = 0, closed = 0, nodes = 0;
                require(sscanf(row.c_str(), "%d %d %d %f %f %d %d %d", &s.id, &s.type, &s.options,
                               &s.spacing, &s.angleOffset, &smooth, &closed, &nodes) == 8 &&
                            (smooth == 0 || smooth == 1) && (closed == 0 || closed == 1) &&
                            nodes > 0 && nodes <= 4096,
                        "Invalid spline record");
                s.smooth = smooth != 0;
                s.closed = closed != 0;
                for (int j = 0; j < nodes; ++j) {
                    next();
                    Vec2 p;
                    require(sscanf(row.c_str(), "%f %f", &p.x, &p.y) == 2, "Invalid spline node");
                    s.nodes.push_back(p);
                }
                validateThingSpline(s);
                require(ids.insert(s.id).second, "Duplicate spline id");
                w.thingSplines.push_back(std::move(s));
            }
        } else if (row == "// DoomEd spline thing owners v1") {
            require(!ownersRead, "Duplicate spline owner section");
            ownersRead = true;
            next();
            int owners = 0;
            require(sscanf(row.c_str(), "%d", &owners) == 1 && owners == int(w.things.size()),
                    "Invalid spline owner count");
            for (auto &t : w.things) {
                next();
                require(sscanf(row.c_str(), "%d", &t.splineId) == 1 &&
                            (t.splineId == 0 || w.findSpline(t.splineId)),
                        "Invalid spline thing owner");
            }
        } else if (row == "// DoomEd reference image v1") {
            next();
            std::istringstream fields(row);
            std::string extra;
            require(bool(fields >> std::quoted(w.reference.path)) && !(fields >> extra) &&
                        !w.reference.path.empty() && w.reference.path.size() <= 4096,
                    "Invalid reference image path");
            next();
            auto &r = w.reference;
            require(sscanf(row.c_str(), "%f %f %f %f %f", &r.center.x, &r.center.y, &r.scale,
                           &r.opacity, &r.rotation) == 5 &&
                        std::isfinite(r.center.x) && std::isfinite(r.center.y) &&
                        std::abs(r.center.x) <= 1000000 && std::abs(r.center.y) <= 1000000 &&
                        std::isfinite(r.scale) && r.scale >= .001f && r.scale <= 1024 &&
                        std::isfinite(r.opacity) && r.opacity >= 0 && r.opacity <= 1 &&
                        std::isfinite(r.rotation) && std::abs(r.rotation) <= 36000,
                    "Invalid reference image transform");
        } else if (row == "// DoomEd sector indices v1") {
            for (auto &l : w.lines) {
                require(bool(std::getline(in, row)), "Truncated sector indices");
                require(sscanf(row.c_str(), "%d %d", &l.side[0].sector, &l.side[1].sector) == 2,
                        "Invalid sector indices");
            }
            w.explicitSectors = true;
        } else if (row == "// DoomEd sector portals v1") {
            throw std::runtime_error("Sector-over-sector maps are not supported by this editor");
        } else if (row == "// DoomEd texture UVs v1") {
            for (auto &l : w.lines)
                for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                    auto &s = l.side[side];
                    for (auto *uv : {&s.uv, &s.ends.floorUV, &s.ends.ceilingUV}) {
                        require(bool(std::getline(in, row)) &&
                                    sscanf(row.c_str(), "%d %d %d %d", &uv->scaleU, &uv->scaleV,
                                           &uv->offsetU, &uv->offsetV) == 4,
                                "Invalid texture UV record");
                        uv->validate();
                    }
                }
        } else if (row == "// DoomEd texture rotations v1") {
            std::vector<SectorRotation> rotations;
            for (auto &l : w.lines)
                for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                    auto &r = l.side[side].ends.rotation;
                    require(bool(std::getline(in, row)) &&
                                sscanf(row.c_str(), "%d %d", &r.floor, &r.ceiling) == 2,
                            "Invalid texture rotation record");
                    rotations.push_back(r);
                }
            auto data = encodeRotations(rotations);
            if (!data.empty())
                decodeRotations(data.data(), data.size(), rotations.size());
        } else if (row == "// DoomEd floor slopes v1" || row == "// DoomEd ceiling slopes v1") {
            const bool ceiling = row == "// DoomEd ceiling slopes v1";
            std::vector<FloorSlope> slopes;
            for (auto &l : w.lines)
                for (int side = 0; side < ((l.flags & 4) ? 2 : 1); ++side) {
                    auto &s = ceiling ? l.side[side].ends.ceilingSlope : l.side[side].ends.slope;
                    require(bool(std::getline(in, row)) &&
                                sscanf(row.c_str(), "%d %d %d %d", &s.x, &s.y, &s.dx, &s.dy) == 4,
                            "Invalid floor slope record");
                    slopes.push_back(s);
                }
            auto bytes = encodeSlopes(slopes);
            if (!bytes.empty())
                decodeSlopes(bytes.data(), bytes.size(), slopes.size());
        }
    }
    require(w.thingSplines.empty() || ownersRead, "Spline section is missing its thing owners");
    w.markSaved();
    *this = std::move(w);
}
void EditWorld::load(const std::filesystem::path &p) {
    auto b = readFile(p);
    parse(std::string(b.begin(), b.end()));
    pathname = p;
    if (!reference.path.empty()) {
        auto image = std::filesystem::u8path(reference.path);
        if (image.is_relative())
            image = std::filesystem::absolute(p).parent_path() / image;
        reference.path = image.lexically_normal().u8string();
    }
    markSaved();
}
void EditWorld::save(const std::filesystem::path &p) {
    auto destination = *this;
    destination.pathname = p;
    writeTextAtomic(p, destination.serialize());
    pathname = p;
    markSaved();
}
} // namespace de
