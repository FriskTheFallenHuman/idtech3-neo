#!/usr/bin/env python3
"""Verify actual WAD pixels, PNGs, animation references and malformed import inputs."""

from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("weapon_import", ROOT / "code/import-doom-weapons.py")
importer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(importer)


class WeaponAssetTests(unittest.TestCase):
    def test_wad_and_pixels(self):
        data = (ROOT / "base/wad/DOOM.WAD").read_bytes()
        metadata = json.loads((ROOT / "base/models/weapons/doom-assets.json").read_text())
        self.assertEqual(hashlib.sha256(data).hexdigest(), metadata["sha256"])
        count, directory = struct.unpack_from("<II", data, 4)
        lumps = {}
        for i in range(count):
            offset, size, name = struct.unpack_from("<II8s", data, directory + i * 16)
            lumps[name.rstrip(b"\0").decode()] = data[offset : offset + size]
        palette = lumps["PLAYPAL"]
        for path in (ROOT / "base/models/weapons").rglob("*.png"):
            name = path.stem.upper()
            lump = lumps[name]
            width, height, left, top = struct.unpack_from("<HHhh", lump)
            with Image.open(path) as image:
                self.assertEqual(image.mode, "RGBA")
                # Independently expand each source column and compare every padded pixel.
                expected = Image.new("RGBA", image.size)
                for x in range(width):
                    pos = struct.unpack_from("<I", lump, 8 + x * 4)[0]
                    while lump[pos] != 255:
                        row, length = lump[pos : pos + 2]
                        for y in range(length):
                            index = lump[pos + 3 + y]
                            color = tuple(palette[index * 3 : index * 3 + 3]) + (255,)
                            expected.putpixel((x, row + y), c
olor)
                        pos += length + 4
                self.assertEqual(image.tobytes(), expected.tobytes(), str(path))
            if path.parent.name != "ammo":
                self.assertEqual(
                    metadata["weapons"][path.parent.name]["patches"][name][:4],
                    [width, height, left, top],
                )

    def test_config_references(self):
        root = ROOT / "base/models/weapons"
        for directory in root.iterdir():
            if not directory.is_dir() or not (directory / "sprites.cfg").exists():
                continue
            tokens = (directory / "sprites.cfg").read_text().split()
            self.assertEqual(tokens[:3], ["doomSprites", "1", "patches"])
            count = int(tokens[3])
            cursor = 4
            for _ in range(count):
                path = ROOT / "base" / (tokens[cursor] + ".png")
                self.assertTrue(path.is_file(), str(path))
                with Image.open(path) as image:
                    self.assertEqual(image.size, tuple(map(int, tokens[cursor + 5 : cursor + 7])))
                cursor += 7
            self.assertEqual(tokens[cursor], "frames")
            frames = int(tokens[cursor + 1])
            cursor += 2
            self.assertEqual(len(tokens) - cursor, frames * 3)
            for _ in range(frames):
                body, flash, offset = map(int, tokens[cursor : cursor + 3])
                self.assertTrue(0 <= body < count and -1 <= flash < count and 0 <= offset <= 96)
                cursor += 3
            if directory.name == "ammo":
                continue
            rows = [
                line.split()
                for line in (directory / "weapon.cfg").read_text().splitlines()
                if line and not line.startswith("//")
            ]
            self.assertEqual(len(rows), 12)
            for row in rows:
                first, length, fps, loop = map(int, row)
                self.assertTrue(0 <= first < frames and 0 < len
gth <= frames - first)
                self.assertTrue(fps == 35 and 0 <= loop <= length)
            attack = importer.WEAPONS[directory.name][1]
            self.assertEqual(
                sum(span for _, span in attack),
                8 if directory.name in ("chainsaw", "chaingun") else int(rows[2][1]),
            )

    def test_deterministic_import(self):
        files = [p for p in (ROOT / "base/models/weapons").rglob("*") if p.is_file()]
        before = {p: hashlib.sha256(p.read_bytes()).digest() for p in files}
        importer.extract()
        self.assertEqual(before, {p: hashlib.sha256(p.read_bytes()).digest() for p in files})

    def test_malformed_wad(self):
        for data in (
            b"",
            b"IWAD",
            b"NOPE" + b"\0" * 8,
            b"IWAD" + struct.pack("<II", 1, 12),
            b"IWAD" + struct.pack("<II", 0xFFFFFFFF, 0),
        ):
            with self.assertRaises(ValueError):
                importer.read_wad(data)

    def test_malformed_patches(self):
        palette = bytes(768)
        header = struct.pack("<HHhhI", 1, 1, 0, 0, 12)
        malformed = (
            b"",
            header[:7],
            struct.pack("<HHhh", 0, 1, 0, 0),
            header,
            header + b"\0",
            header + b"\0\x02\0\x01\x01\0\xff",
            header + b"\0\x01\0\x01\0",
        )
        for data in malformed:
            with self.assertRaises(ValueError):
                importer.decode_patch(data, palette)
        # Real zero pixels remain opaque; only absent posts produce transparent pixels.
        values, rgba = importer.decode_patch(header + b"\0\x01\0\0\0\xff", palette)
        self.assertEqual(values, (1, 1, 0, 0, 1, 1))
        self.assertEqual(rgba, b"\0\0\0\xff")


if __name__ == "__main__":
    unittest.main()
