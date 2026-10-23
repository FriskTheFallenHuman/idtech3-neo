"""Verify all Doom pickup PNGs against original WAD pixels and asset references."""

import importlib.util
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[3]


def main():
    spec = importlib.util.spec_from_file_location(
        "doom_monsters", ROOT / "code/import-doom-monsters.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    helper = module.weapon_importer()
    lumps = helper.read_wad((ROOT / "base/wad/DOOM.WAD").read_bytes())
    definitions = json.loads((ROOT / "base/editor/doom-items.json").read_text())
    count = 0
    for item in definitions:
        folder = ROOT / f"base/models/items/{item['directory']}"
        cfg = (folder / "animation.cfg").read_text()
        for png in (folder / "frames").glob("*.png"):
            metadata, rgba = helper.decode_patch(lumps[png.stem.upper()], lumps["PLAYPAL"][:768])
            data = png.read_bytes()
            assert data[:8] == b"\x89PNG\r\n\x1a\n"
            assert struct.unpack_from(">II", data, 16) == metadata[4:6]
            compressed = bytearray()
            offset = 8
            while offset < len(data):
                size, kind = struct.unpack_from(">I4s", data, offset)
                if kind == b"IDAT":
                    compressed.extend(data[offset + 8 : offset + 8 + size])
                offset += size + 12
            stride = metadata[4] * 4
            expected = b"".join(
                b"\0" + rgba[y * stride : (y + 1) * stride] for y in range(metadata[5])
            )
            assert zlib.decompress(compressed) == expected, png
            assert f"models/items/{item['directory']}/frames/{png.stem} " in cfg
            count += 1
    assert len(definitions) == 34
    print(
        f"Verified {count} original pickup PNGs in 34 item directories and their config references."
    )


if __name__ == "__main__":
    main()
