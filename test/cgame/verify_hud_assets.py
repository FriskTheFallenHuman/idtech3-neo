"""Check HUD border assets against original WAD pixels and reject malformed flats."""

import importlib.util
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("menu_import", ROOT / "code/import-doom-menu.py")
IMPORTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(IMPORTER)


class HudAssetTests(unittest.TestCase):
    def test_original_background(self):
        wad = (ROOT / "base/wad/DOOM.WAD").read_bytes()
        count, directory = struct.unpack_from("<II", wad, 4)
        lumps = {}
        for index in range(count):
            offset, size, name = struct.unpack_from("<II8s", wad, directory + index * 16)
            lumps[name.rstrip(b"\0").decode()] = wad[offset : offset + size]
        image = (ROOT / "base/ui/doom/floor7_2.tga").read_bytes()
        self.assertEqual(struct.unpack_from("<HHB", image, 12), (64, 64, 32))
        self.assertEqual(len(image), 18 + 64 * 64 * 4)
        palette = lumps["PLAYPAL"]
        for y in range(64):
            for x in range(64):
                index = lumps["FLOOR7_2"][y * 64 + x]
                red, green, blue = palette[index * 3 : index * 3 + 3]
                offset = 18 + ((63 - y) * 64 + x) * 4
                self.assertEqual(image[offset : offset + 4], bytes((blue, green, red, 255)))
        shader = (ROOT / "base/scripts/doom_menu.shader").read_text()
        background = shader[shader.index("ui/doom/floor7_2\n") :]
        self.assertIn("map ui/doom/floor7_2.tga", background)
        self.assertNotIn("clampmap", background)
        self.assertIn("nopicmip", background)
        self.assertIn("nomipmaps", background)
        metadata = (ROOT / "code/qcommon/doom_ui_assets.h").read_text()
        self.assertIn('{"FLOOR7_2", 64, 64, 0, 0, 64, 64, 0}', metadata)

    def test_malformed_background(self):
        for pixels, palette in (
            (b"", bytes(768)),
            (bytes(4095), bytes(768))
,
            (bytes(4097), bytes(768)),
            (bytes(4096), bytes(767)),
        ):
            with self.subTest(size=len(pixels), palette=len(palette)):
                with self.assertRaises(ValueError):
                    IMPORTER.flat_image(pixels, palette)


if __name__ == "__main__":
    unittest.main()
