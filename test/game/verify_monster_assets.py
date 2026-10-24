"""Verify installed sprites against the original WAD and exercise importer errors."""

from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import wave
import zlib
from collections import Counter

ROOT = Path(__file__).resolve().parents[3]


def main():
    script = ROOT / "code/import-doom-monsters.py"
    spec = importlib.util.spec_from_file_location("doom_monsters", script)
    importer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(importer)
    helper = importer.weapon_importer()
    wad = (ROOT / "base/wad/DOOM.WAD").read_bytes()
    lumps = helper.read_wad(wad)
    palette = lumps["PLAYPAL"][:768]
    lump_count, directory = struct.unpack_from("<ii", wad, 4)
    entries = [struct.unpack_from("<ii8s", wad, directory + i * 16) for i in range(lump_count)]
    monster_types = {3004, 9, 3001, 3002, 58, 3006, 3005, 3003, 16, 7}
    for bsp in (ROOT / "base/maps").glob("*.bsp"):
        marker = next(
            i
            for i, entry in enumerate(entries)
            if entry[2].rstrip(b"\0").decode() == bsp.stem.upper()
        )
        offset, size, _ = entries[marker + 1]
        original = Counter(
            t
            for t in struct.iter_unpack("<5h", wad[offset : offset + size])
            if t[3] in monster_types
        )
        data = bsp.read_bytes()
        offset, size = struct.unpack_from("<ii", data, 8 + 21 * 8)
        converted = Counter(
            t[:5]
            for t in struct.iter_unpack("<6i", data[offset : offset + size])
            if t[3] in monster_types
        )
        assert original == converted, f"Original monster positions/options differ: {bsp.stem}"
    manifest = json.loads((ROOT / "base/models/players/doom-monsters.json").read_text())
    assert hashlib.sha256(wad).hexdigest() == manifest["wad_sha256"]
    assert len(manifest["actors"]) == 17
    patches = 0
    for actor, record in manifest["actors"].items():
        for name in record["patches"]:
            metadata, rgba = helper.decode_patch(lumps[name], palette)
            png = (ROOT / f"base/models/players/{actor}/frames/{name.lower()}.png").read_bytes()
            assert png[:8] == b"\x89PNG\r\n\x1a\n"
            assert struct.unpack_from(">II", png, 16) == metadata[4:6]
            compressed = bytearray()
            offset = 8
            while offset < len(png):
                size, kind = struct.unpack_from(">I4s", png, offset)
                if kind == b"IDAT":
                    compressed.extend(png[offset + 8 : offset + 8 + size])
                offset += 12 + size
            scanlines = zlib.decompress(compressed)
            stride = metadata[4] * 4
            expected = b"".join(
                b"\0" + rgba[y * stride : (y + 1) * stride] for y in range(metadata[5])
            )
            assert scanlines == expected, f"Sprite pixel mismatch: {actor}/{name}"
            patches += 1
    sounds = list((ROOT / "base/sound/doom/monsters").glob("*.wav"))
    assert len(sounds) == manifest["sounds"] == 67
    for sound in sounds:
        with wave.open(str(sound), "rb") as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 22050)
            source = lumps["DS" + sound.stem.upper()]
            rate, samples = struct.unpack_from("<HI", source, 2)
            assert wav.getnframes() == samples * 22050 // rate
    views = importer.rotations(["TROOA1", "TROOA2A8"], "TROO")
    assert views[0, 1] == (1, 0) and views[0, 7] == (1, 1)
    for invalid in (["TROOA1", "TROOA1"], ["TROOA9"], ["TROOB"]):
        try:
            importer.rotations(invalid, "TROO")
        except ValueError:
            pass
        else:
            raise AssertionError(f"Importer accepted malformed rotations: {invalid}")
    print(f"Verified {patches} sprite PNGs against WAD pixels and {len(sounds)} PCM sounds.")


if __name__ == "__main__":
    main()
