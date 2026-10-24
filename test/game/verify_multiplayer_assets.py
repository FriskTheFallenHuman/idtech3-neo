"""Verify Doom menu typography and red/blue marine palette translations."""

from pathlib import Path
import struct
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]


class MultiplayerAssets(unittest.TestCase):
    def test_menu_lettering(self):
        with (
            Image.open(ROOT / "base/ui/doom/m_multi.tga") as multiplayer,
            Image.open(ROOT / "base/ui/doom/m_messg.tga") as messages,
        ):
            self.assertEqual(
                multiplayer.crop((0, 0, 15, 15)).tobytes(), messages.crop((0, 0, 15, 15)).tobytes()
            )
            self.assertEqual(multiplayer.getbbox()[3], 15)
            colors = {pixel[:3] for pixel in multiplayer.get_flattened_data() if pixel[3]}
            self.assertGreater(len(colors), 5, "Retain original bevel shading")
        metadata = (ROOT / "code/qcommon/doom_ui_assets.h").read_text()
        self.assertIn('{"M_MULTI",', metadata)

    def test_team_sprites(self):
        wad = (ROOT / "base/wad/DOOM.WAD").read_bytes()
        count, directory = struct.unpack_from("<II", wad, 4)
        for index in range(count):
            offset, size, name = struct.unpack_from("<II8s", wad, directory + index * 16)
            if name.rstrip(b"\0") == b"PLAYPAL":
                palette = [tuple(wad[offset + i * 3 : offset + i * 3 + 3]) for i in range(256)]
                break
        else:
            self.fail("Missing installed palette")
        shader = (ROOT / "base/scripts/doom-monsters.shader").read_text()
        frames = list((ROOT / "base/models/players/doomguy/frames").glob("*.png"))
        self.assertGreater(len(frames), 20)
        changed = {"red": 0, "blue": 0}
        for path in frames:
            with Image.open(path) as source:
                original = list(source.convert("RGBA").get_flattened_data())
                for team, base in (("red", 176), ("blue", 192)):
                    target = path.parent.parent / f"frames_{team}" / path.name
                    self.assertIn(
                        f"clampmap models/players/doomguy/frames_{team}/{path.name}", shader
                    )
                    with Image.open(target) as translated:
                        self.assertEqual(source.size, translated.size)
                        for before, after in zip(
                            original, translated.convert("RGBA").get_flattened_data()
                        ):
                            if before[3] and before[:3] in palette[112:128]:
                                expected = palette[base + palette[112:128].index(before[:3])]
                                self.assertEqual(after, (*expected, before[3]))
                                changed[team] += 1
                            else:
                                self.assertEqual(
                                    after, before, "Skin, weapons and alpha stay original"
                                )
        self.assertGreater(changed["red"], 1000)
        self.assertEqual(changed["red"], changed["blue"])


if __name__ == "__main__":
    unittest.main()
