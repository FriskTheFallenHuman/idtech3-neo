"""Independently inspect compiler outputs produced by the integrated editor host test."""

import argparse
import importlib.util
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import run_compiler


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True)
    args = parser.parse_args()
    project = args.project.resolve()
    bsp_tools = load_module("bsp_verifier", ROOT / "code/tools/compilers/dmap/tests/verify.py")
    aas_tools = load_module("aas_verifier", ROOT / "code/build-aas.py")
    maps = project / "base/maps"
    bsp_path = maps / "worktest.bsp"
    bsp = bsp_tools.Bsp(bsp_path)
    if bsp.solid(0, (256, 256, 64)) or not bsp.solid(0, (256, 256, -16)):
        raise RuntimeError("Compiled room has invalid floor collision")
    if not bsp.solid(0, (-1, 256, 64)) or not bsp.solid(0, (256, 256, 272)):
        raise RuntimeError("Compiled room has invalid wall or ceiling collision")
    checksum = aas_tools.bsp_checksum(bsp_path.read_bytes())
    profiles = []
    for world in range(4):
        record = aas_tools.inspect_aas(maps / f"worktest_b{world}.aas")
        if record["bsp_checksum"] != checksum:
            raise RuntimeError("Navigation world references another BSP")
        profiles.append(record)
    textures = project / "base/textures/worktest"
    expected = bsp_tools.expected_wall(project / "base/wad/DOOM.WAD", "STARTAN3")
    if bsp_tools.png(textures / "startan3.png") != expected:
        raise RuntimeError("Wall texture assigned to a floor has incorrect pixels")
    if not (textures / "floor0_1.png").is_file():
        raise RuntimeError("Floor texture assigned to a wall is missing")
    rejected = project / "out/rejected"
    rejected.mkdir(parents=True, exist_ok=True)
    for source, data in (
        ("short.bsp", b"IBSP"),
        ("empty.bsp", b""),
        ("invalid.bsp", struct.pack("<4si", b"IBSP", 999) + bytes(180)),
    ):
        path = rejected / source
        path.write_bytes(data)
        result = run_compiler(
            ROOT / "toolsx86.dll", "aasbuild", ["--output", str(rejected), str(path)]
        )
        if result.returncode == 0:
            raise RuntimeError(f"Malformed BSP accepted: {source}")
    if list(rejected.glob("*.aas")):
        raise RuntimeError("Malformed BSP emitted navigation data")
    report = {
        "checks": [
            "independent BSP collision",
            "unified texture pixels",
            "AAS checksums and bounds",
            "malformed BSP rejection",
        ],
        "profiles": profiles,
    }
    (project / "out/compiler-verification.json").write_text(json.dumps(report, indent=2))
    print("Compiler outputs passed independent geometry/collision, texture and navigation checks")


if __name__ == "__main__":
    main()
