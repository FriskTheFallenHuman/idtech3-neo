"""Offline integration tests. Runs the compiler, never the engine.

py code/tools/compilers/dmap/tests/verify.py --compiler toolsx86.dll --base base --out out/doommap-tests
"""

import argparse
import hashlib
import json
import math
import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import run_compiler

SIZES = [1, 72, 16, 36, 48, 4, 4, 40, 12, 8, 44, 4, 72, 104, 1, 1, 1, 68, 36, 48, 16, 24]


def check(value, message):
    if not value:
        raise AssertionError(message)


class Bsp:
    def __init__(self, path):
        self.path = path
        self.raw = path.read_bytes()
        check(len(self.raw) >= 184, f"{path}: short header")
        check(struct.unpack_from("<4si", self.raw) == (b"IBSP", 48), "BSP version")
        self.lumps = [self.raw[o : o + n] for o, n in struct.iter_unpack("<ii", self.raw[8:184])]
        for i, (o, n) in enumerate(struct.iter_unpack("<ii", self.raw[8:184])):
            check(
                o >= 184 and n >= 0 and o + n <= len(self.raw) and o % 4 == 0 and n % SIZES[i] == 0,
                f"lump {i} bounds",
            )
        self.count = [len(x) // n for x, n in zip(self.lumps, SIZES)]
        self.sectors = list(struct.iter_unpack("<7i12s12s4f", self.lumps[17]))
        self.models = list(struct.iter_unpack("<6f4i", self.lumps[7]))
        self.surfaces = list(struct.iter_unpack("<12i12f2i", self.lumps[13]))
        self.vertices = list(struct.iter_unpack("<10f4B", self.lumps[10]))
        self.indices = [x[0] for x in struct.iter_unpack("<i", self.lumps[11])]
        self.surface_sectors = list(struct.iter_unpack("<4i", self.lumps[20]))
        self.entities = [
            dict(re.findall(r'"([^"]+)"\s+"([^"]*)"', e))
            for e in re.findall(r"\{([^}]+)\}", self.lumps[0].decode("ascii"))
        ]
        self.brushes = list(struct.iter_unpack("<3i", self.lumps[8]))
        self.brush_sides = list(struct.iter_unpa
ck("<2i", self.lumps[9]))
        self.planes = list(struct.iter_unpack("<4f", self.lumps[2]))
        self.validate()

    def validate(self):
        check(not self.lumps[14] and not self.lumps[15], "baked lighting present")
        check(len(self.surface_sectors) == len(self.surfaces), "surface-sector mapping")
        check(len(self.models) <= 256 and len(self.entities) + 64 < 1022, "runtime limits")
        for i, (surface, metadata) in enumerate(zip(self.surfaces, self.surface_sectors)):
            shader, fog, kind, first, count, index, nindex, lightmap = surface[:8]
            check(
                0 <= shader < self.count[1] and fog == -1 and kind == 3 and lightmap == -3,
                "surface format",
            )
            check(0 <= metadata[0] < len(self.sectors), "surface sector")
            check(0 <= first <= first + count <= len(self.vertices), "vertex range")
            check(
                0 <= index <= index + nindex <= len(self.indices) and nindex % 3 == 0, "index range"
            )
            brightness = self.sectors[metadata[0]][2]
            for v in self.vertices[first : first + count]:
                check(all(math.isfinite(x) for x in v[:10]), "non-finite draw vertex")
                check(
                    v[10:] == (brightness, brightness, brightness, 255), "sector brightness changed"
                )
            for j in range(index, index + nindex, 3):
                triangle = self.indices[j : j + 3]
                check(all(0 <= x < count for x in triangle), "triangle index")
                a, b, c = [self.vertices[first + x][:3] for x in triangle]
                u = [b[k] - a[k] for k in range(3)]
                v = [c[k] - a[k] for k in range(3)]
                area = sum(
                    x * x
                    for x in (
                        u[1] * v[2] - u[2] * v[1],
                        u[2] * v[0] - u[0] * v[2],
                        u[0] * v[1] - u[1] * v[0],
                    )
 
               )
                check(area > 1e-10, f"degenerate triangle on surface {i}")
        for first, count, shader in self.brushes:
            check(
                count >= 6 and 0 <= first <= first + count <= len(self.brush_sides), "brush range"
            )
            check(0 <= shader < self.count[1], "brush shader")
            for plane, material in self.brush_sides[first : first + count]:
                check(0 <= plane < len(self.planes) and 0 <= material < self.count[1], "brush side")
        for i, p in enumerate(self.planes):
            opposite = self.planes[i ^ 1]
            check(all(abs(x + y) < 1e-4 for x, y in zip(p, opposite)), "unpaired plane")
        for model in self.models:
            sf, ns, br, nb = model[6:]
            check(
                0 <= sf <= sf + ns <= len(self.surfaces)
                and 0 <= br <= br + nb <= len(self.brushes),
                "model range",
            )
        for sector in self.sectors:
            check(sector[0] <= sector[1] and 0 <= sector[2] <= 255, "invalid sector")
            for model in sector[5:7]:
                check(model == -1 or 1 <= model < len(self.models), "sector model")

    def floor_area(self):
        area = 0
        for s, meta in zip(self.surfaces, self.surface_sectors):
            if meta[2] != 1:
                continue
            for j in range(s[5], s[5] + s[6], 3):
                a, b, c = [self.vertices[s[3] + x] for x in self.indices[j : j + 3]]
                area += abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2
        return area

    def solid(self, model, point, offset=0):
        m = self.models[model]
        for first, count, _ in self.brushes[m[8] : m[8] + m[9]]:
            if all(
                sum(
                    n * p
                    for n, p in zip(self.planes[plane][:3], (point[0], point[1], point[2] - offset))
                )
                <= self.planes[plane][3] + 1e-4
                for pla
ne, _ in self.brush_sides[first : first + count]
            ):
                return True
        return False


def png(path):
    b = path.read_bytes()
    check(b[:8] == b"\x89PNG\r\n\x1a\n", "PNG signature")
    p, compressed, width, height = 8, bytearray(), 0, 0
    while p < len(b):
        n = struct.unpack_from(">I", b, p)[0]
        kind, payload = b[p + 4 : p + 8], b[p + 8 : p + 8 + n]
        check(zlib.crc32(kind + payload) == struct.unpack_from(">I", b, p + 8 + n)[0], "PNG CRC")
        if kind == b"IHDR":
            width, height, depth, color, compression, filter_, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            check((depth, color, compression, filter_, interlace) == (8, 6, 0, 0, 0), "PNG format")
        if kind == b"IDAT":
            compressed.extend(payload)
        p += n + 12
    raw = zlib.decompress(compressed)
    check(len(raw) == height * (width * 4 + 1), "PNG scanline size")
    rgba = bytearray()
    for y in range(height):
        row = raw[y * (width * 4 + 1) : (y + 1) * (width * 4 + 1)]
        check(row[0] == 0, "unexpected PNG filter")
        rgba.extend(row[1:])
    return width, height, bytes(rgba)


def expected_wall(wad, name):
    """Independent compositing oracle for patch overlaps, offsets and palette."""
    data = wad.read_bytes()
    count, directory = struct.unpack_from("<II", data, 4)
    lumps = {}
    for i in range(count):
        offset, size, n = struct.unpack_from("<II8s", data, directory + i * 16)
        lumps[n.rstrip(b"\0").decode("ascii").upper()] = data[offset : offset + size]
    palette = lumps["PLAYPAL"][:768]
    names = lumps["PNAMES"]
    patches = [
        names[4 + i * 8 : 12 + i * 8].rstrip(b"\0").decode("ascii")
        for i in range(struct.unpack_from("<I", names)[0])
    ]
    for label in ("TEXTURE1", "TEXTURE2"):
        if label not in lumps:
            continue
        table = lumps[label]
        for i in range(struct.unpack_from("<I", table)[0
]):
            offset = struct.unpack_from("<I", table, 4 + i * 4)[0]
            if table[offset : offset + 8].rstrip(b"\0").decode("ascii") != name:
                continue
            width, height, _, count = struct.unpack_from("<HHIH", table, offset + 12)
            result = bytearray(width * height * 4)
            for j in range(count):
                ox, oy, index, _, _ = struct.unpack_from("<hhhhh", table, offset + 22 + j * 10)
                patch = lumps[patches[index]]
                pw, ph = struct.unpack_from("<HH", patch)
                for x in range(pw):
                    q = struct.unpack_from("<I", patch, 8 + x * 4)[0]
                    previous = -1
                    while patch[q] != 255:
                        top, length = patch[q : q + 2]
                        if top <= previous:
                            top += previous
                        previous = top
                        for y in range(length):
                            dx, dy = x + ox, top + y + oy
                            if 0 <= dx < width and 0 <= dy < height:
                                color = patch[q + 3 + y]
                                result[(dy * width + dx) * 4 : (dy * width + dx) * 4 + 4] = (
                                    palette[color * 3 : color * 3 + 3] + b"\xff"
                                )
                        q += length + 4
            return width, height, bytes(result)
    raise AssertionError(f"WAD texture {name} missing")


def sector(floor=0, ceiling=128, light=128, tag=0):
    return f"{floor} : FLOOR4_8 {ceiling} : CEIL3_5 {light} 0 {tag}"


def line(a, b, front, back=None, special=0):
    return (
        f'({a[0]},{a[1]}) to ({b[0]},{b[1]}) : {4 if back else 1} : {special} : 0\n    0 (0 : STARTAN3 / STARTAN3 / {"-" if back else "STARTAN3"} )\n    {front}\n'
        + (f"    0 (0 : STARTAN3 / STARTAN3 / - )\n    {back}\n" if back else "")
    )


def loop(points, definition):
    return [line(a, b, definition)
 for a, b in zip(points, points[1:] + points[:1])]


def fixture(path, lines):
    path.write_text(
        "WorldServer version 4\n\nlines:"
        + str(len(lines))
        + "\n"
        + "".join(lines)
        + "\nthings:1\n(32,32, 90) :1, 7\n"
    )


def run(compiler, root, wad, source, good=True, extra=()):
    result = run_compiler(
        compiler, "dmap", ["--base", str(root), "--wad", str(wad), *extra, str(source)]
    )
    check((result.returncode == 0) == good, result.stdout + result.stderr)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    compiler, base, output = args.compiler.resolve(), args.base.resolve(), args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    wad = next((base / "wad").glob("*.WAD"), None) or next((base / "wad").glob("*.wad"))
    maps, images = [], 0
    jambs_checked = 0
    for source in sorted((base / "maps").glob("*.dwd")):
        if not re.fullmatch(r"e[1-3]m[1-9]", source.stem, re.IGNORECASE):
            continue
        bsp = Bsp(source.with_suffix(".bsp"))
        # Walls exposed by moving sectors must exist in the static world,
        # including zero-height door tracks and the lower walls of lift shafts.
        movers = {
            (int(e["doom_sector"]), int(e["doom_plane"])): e
            for e in bsp.entities
            if e["classname"] == "doom_mover"
        }
        world_end = bsp.models[0][6] + bsp.models[0][7]
        sides = list(struct.iter_unpack("<3i12s12s12s", bsp.lumps[19]))
        for li, line_data in enumerate(struct.iter_unpack("<4f5i", bsp.lumps[18])):
            side_ids = line_data[-2:]
            if side_ids[1] >= 0:
                continue
            side = sides[side_ids[0]]
            sec = side[2]
            if (sec, 0) not in
 movers and (sec, 1) not in movers:
                continue
            if side[5].split(b"\0")[0] == b"-":
                continue
            floor, ceiling = bsp.sectors[sec][:2]
            lo = (
                min(floor, float(movers[(sec, 0)]["doom_lowfloor"]))
                if (sec, 0) in movers
                else floor
            )
            hi = (
                max(ceiling, float(movers[(sec, 1)]["doom_highceil"]))
                if (sec, 1) in movers
                else ceiling
            )
            if hi <= lo:
                continue
            matches = [
                i for i, meta in enumerate(bsp.surface_sectors) if meta[1] == li and meta[2] == 0
            ]
            check(
                matches and all(i < world_end for i in matches),
                f"{source.stem}: jamb on moving model, line {li}",
            )
            zs = [
                v[2]
                for i in matches
                for v in bsp.vertices[bsp.surfaces[i][3] : bsp.surfaces[i][3] + bsp.surfaces[i][4]]
            ]
            check(
                min(zs) <= lo and max(zs) >= hi, f"{source.stem}: missing exposed wall, line {li}"
            )
            jambs_checked += 1
        shader = base / "scripts" / (source.stem + ".shader")
        check(shader.is_file(), "map shader missing")
        for image in set(re.findall(r"textures/[^\s{}]+\.png", shader.read_text())):
            check((base / image).is_file(), f"shader references missing image: {image}")
        for image in (base / "textures" / source.stem).glob("*.png"):
            png(image)
            images += 1
        maps.append(
            {
                "map": source.stem,
                "sectors": len(bsp.sectors),
                "models": len(bsp.models),
                "surfaces": len(bsp.surfaces),
                "brushes": len(bsp.brushes),
                "entities": len(bsp.entities),
            }
        )
    check(len(maps) == 27, "expected all 27 sour
ce maps")
    root = output / "fixtures"
    root.mkdir(exist_ok=True)
    outer = [(0, 0), (0, 256), (256, 256), (256, 0)]
    hole = [(96, 96), (160, 96), (160, 160), (96, 160)]
    path = root / "hole.dwd"
    fixture(path, loop(outer, sector()) + loop(hole, sector()))
    run(compiler, root, wad, path, extra=("--strict",))
    bsp = Bsp(root / "maps/hole.bsp")
    check(
        len(bsp.sectors) == 1 and abs(bsp.floor_area() - (256 * 256 - 64 * 64)) < 0.001,
        "hole decomposition/area",
    )
    check(bsp.solid(0, (32, 32, -1)) and not bsp.solid(0, (32, 32, 32)), "floor collision")
    check(not bsp.solid(0, (128, 128, -1)), "hole incorrectly filled")
    snapshot = {
        p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in root.rglob("*")
        if p.is_file()
    }
    (root / "scripts/hole.shader").write_text("STALE SHADER MUST BE REPLACED")
    run(compiler, root, wad, path)
    for name, digest in snapshot.items():
        check(
            hashlib.sha256((root / name).read_bytes()).hexdigest() == digest,
            f"non-deterministic output: {name}",
        )
    # Disconnected rooms with equal properties must remain separate sectors.
    path = root / "disconnected.dwd"
    second = [(x + 384, y) for x, y in outer]
    fixture(path, loop(outer, sector()) + loop(second, sector()))
    run(compiler, root, wad, path, extra=("--strict",))
    check(len(Bsp(root / "maps/disconnected.bsp").sectors) == 2, "disconnected sectors merged")
    # A closed Doom door sits between two rooms. It must not be world geometry.
    path = root / "door.dwd"
    left, door, right = sector(light=144), sector(ceiling=0, light=192), sector(light=224)
    lines = [
        line((0, 0), (0, 128), left),
        line((0, 128), (128, 128), left),
        line((128, 0), (0, 0), left),
        line((128, 128), (128, 0), left, door, 1),
        line((128, 128), (160, 128), door),
        line((160, 0), (128, 0), door),
        line((
160, 128), (160, 0), door, right),
        line((160, 128), (288, 128), right),
        line((288, 128), (288, 0), right),
        line((288, 0), (160, 0), right),
    ]
    fixture(path, lines)
    run(compiler, root, wad, path, extra=("--strict",))
    bsp = Bsp(root / "maps/door.bsp")
    mover = next(e for e in bsp.entities if e["classname"] == "doom_mover")
    model = int(mover["model"][1:])
    check(mover["doom_plane"] == "1" and float(mover["doom_lowceil"]) == 128, "door destination")
    check(
        bsp.solid(model, (144, 64, 32)) and not bsp.solid(0, (144, 64, 32)),
        "door not isolated into submodel",
    )
    check(not bsp.solid(model, (144, 64, 32), offset=124), "translated open door blocks passage")
    check(
        any(e["classname"] == "doom_line" and e["doom_special"] == "1" for e in bsp.entities),
        "door activation lost",
    )
    for li in (4, 5):
        faces = [i for i, meta in enumerate(bsp.surface_sectors) if meta[1] == li]
        check(faces and all(i < bsp.models[0][7] for i in faces), "door track moves with ceiling")
        zs = [
            v[2]
            for i in faces
            for v in bsp.vertices[bsp.surfaces[i][3] : bsp.surfaces[i][3] + bsp.surfaces[i][4]]
        ]
        check(min(zs) == 0 and max(zs) == 128, "closed door lost jamb wall geometry")
    # Two independent switches share a WAD texture and control the tagged door.
    path = root / "switches.dwd"
    switch_lines = [
        l.replace(sector(ceiling=0, light=192), sector(ceiling=0, light=192, tag=7)) for l in lines
    ]
    for li, special in ((0, 63), (1, 103)):
        switch_lines[li] = (
            switch_lines[li]
            .replace(": 1 : 0 : 0", f": 1 : {special} : 7")
            .replace("STARTAN3 / STARTAN3 / STARTAN3", "STARTAN3 / STARTAN3 / SW1BRCOM")
        )
    fixture(path, switch_lines)
    run(compiler, root, wad, path, extra=("--strict",))
    switches = Bsp(root / "maps/switches.bsp")
    check(
        sum(e.get("d
oom_switch") == "1" for e in switches.entities) == 2, "switch metadata missing"
    )
    check(
        (root / "textures/switches/sw1brcom.png").is_file()
        and (root / "textures/switches/sw2brcom.png").is_file(),
        "switch texture partner missing",
    )
    # Lift floors must have independent collision, while ceilings stay in the world.
    path = root / "lift.dwd"
    low, raised = sector(floor=0, light=144), sector(floor=64, light=192, tag=7)
    lift = [
        line((0, 0), (0, 128), low),
        line((0, 128), (128, 128), low),
        line((128, 0), (0, 0), low),
        line((128, 128), (128, 0), low, raised, 62),
        line((128, 128), (256, 128), raised),
        line((256, 128), (256, 0), raised),
        line((256, 0), (128, 0), raised),
    ]
    lift[3] = lift[3].replace(": 62 : 0", ": 62 : 7")
    fixture(path, lift)
    run(compiler, root, wad, path, extra=("--strict",))
    bsp = Bsp(root / "maps/lift.bsp")
    mover = next(e for e in bsp.entities if e["classname"] == "doom_mover")
    model = int(mover["model"][1:])
    check(mover["doom_plane"] == "0" and float(mover["doom_lowfloor"]) == 0, "lift destination")
    check(
        bsp.solid(model, (192, 64, 63)) and not bsp.solid(model, (192, 64, 63), offset=-64),
        "lift collision translation",
    )
    # A lift shaft beside a higher neighbor exposes wall below its start floor.
    path = root / "shaft.dwd"
    high = sector(floor=96, light=224)
    shaft = lift[:4] + [
        line((128, 128), (256, 128), raised),
        line((256, 0), (128, 0), raised),
        line((256, 128), (256, 0), raised, high),
        line((256, 128), (384, 128), high),
        line((384, 128), (384, 0), high),
        line((384, 0), (256, 0), high),
    ]
    fixture(path, shaft)
    run(compiler, root, wad, path, extra=("--strict",))
    shaft_bsp = Bsp(root / "maps/shaft.bsp")
    surfaces = [
        i for i, meta in enumerate(shaft_bsp.surface_sectors) if meta[1] == 6 and meta[0] == 1
    ]

    heights = [
        vertex[2]
        for i in surfaces
        for vertex in shaft_bsp.vertices[
            shaft_bsp.surfaces[i][3] : shaft_bsp.surfaces[i][3] + shaft_bsp.surfaces[i][4]
        ]
    ]
    check(
        heights and min(heights) == 0 and max(heights) == 96,
        "lift shaft wall does not cover the lowest stop",
    )
    # Sky rendering extends beyond finite sector ceilings; collision stays original.
    path = root / "sky.dwd"
    fixture(path, loop(outer, sector().replace("CEIL3_5", "F_SKY1")))
    run(compiler, root, wad, path, extra=("--strict",))
    sky = Bsp(root / "maps/sky.bsp")
    sky_heights = [
        v[2]
        for i, meta in enumerate(sky.surface_sectors)
        if meta[2] == 2
        for v in sky.vertices[sky.surfaces[i][3] : sky.surfaces[i][3] + sky.surfaces[i][4]]
    ]
    check(
        sky_heights and max(sky_heights) > 128 and sky.solid(0, (32, 32, 129)),
        "sky portal or original ceiling collision missing",
    )
    # Stair actions must include connected untagged sectors and retain step heights.
    path = root / "stairs.dwd"
    first, second, third = sector(light=144, tag=7), sector(light=192), sector(light=224)
    stairs = [
        line((0, 0), (0, 128), first, special=7),
        line((0, 128), (128, 128), first),
        line((128, 0), (0, 0), first),
        line((128, 128), (128, 0), first, second),
        line((128, 128), (256, 128), second),
        line((256, 0), (128, 0), second),
        line((256, 128), (256, 0), second, third),
        line((256, 128), (384, 128), third),
        line((384, 128), (384, 0), third),
        line((384, 0), (256, 0), third),
    ]
    stairs[0] = stairs[0].replace(": 7 : 0", ": 7 : 7")
    fixture(path, stairs)
    run(compiler, root, wad, path, extra=("--strict",))
    bsp = Bsp(root / "maps/stairs.bsp")
    movers = [e for e in bsp.entities if e["classname"] == "doom_mover"]
    check(
        len(movers) == 3 and sorted(float(e["doom_stairheight"]) for e 
in movers) == [8, 16, 24],
        "stair chain targets",
    )
    check(all(e["doom_linkedtag"] == "7" for e in movers), "untagged stair chain activation")
    # Original editor flags can be negative; preserve their signed source bits.
    path = root / "signed_flags.dwd"
    signed = loop(outer, sector())
    signed[0] = signed[0].replace(": 1 : 0 : 0", ": -255 : 0 : 0")
    fixture(path, signed)
    run(compiler, root, wad, path, extra=("--strict",))
    bsp = Bsp(root / "maps/signed_flags.bsp")
    check(struct.unpack_from("<4f5i", bsp.lumps[18])[4] == -255, "signed DWD flags lost")
    # Independent WAD patch/palette composition must match generated PNG pixels.
    check(
        png(root / "textures/hole/startan3.png") == expected_wall(wad, "STARTAN3"),
        "WAD texture compositing",
    )
    check(
        png(base / "textures/e1m1/door3.png") == expected_wall(wad, "DOOR3"),
        "DOOR3 palette/patch pixels",
    )
    (root / "invalid.map").write_text("{ // legacy Quake input\n}\n")
    run(compiler, root, wad, root / "invalid.map", good=False)
    (root / "bad.dwd").write_text("WorldServer version 3\nlines:0\nthings:0\n")
    run(compiler, root, wad, root / "bad.dwd", good=False)
    check(not (root / "maps/bad.bsp").exists(), "invalid source emitted BSP")
    # Known malformed original sectors must be diagnosed and rejected in strict mode.
    run(compiler, root, wad, base / "maps/e1m2.dwd", good=False, extra=("--strict",))
    report = {
        "maps": maps,
        "pngs_checked": images,
        "exposed_walls_checked": jambs_checked,
        "checks": [
            "all BSP records and triangle ranges",
            "27 map shader/image manifests",
            "PNG CRC, zlib and RGBA scanlines",
            "sector brightness",
            "concave region with hole",
            "disconnected equal sectors",
            "door submodel collision and open translation",
            "stationary door jambs and lift shaft walls across all maps",
   
         "switch state metadata and paired textures",
            "lift floor collision and lowered translation",
            "connected untagged stair chain",
            "signed editor linedef flags",
            "WAD patch/palette pixel oracle",
            "determinism and stale shader replacement",
            "non-DWD and malformed source rejection",
            "strict original-map diagnostics",
        ],
        "engine_launched": False,
    }
    (output / "verification.json").write_text(json.dumps(report, indent=2))
    print(
        f"PASS: {len(maps)} BSPs, {images} PNGs, geometry/collision/asset/determinism fixtures; engine not launched."
    )


if __name__ == "__main__":
    main()
