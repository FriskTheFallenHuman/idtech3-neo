"""Verify MUS conversion event timing, percussion, programs and malformed scores."""

from pathlib import Path
import importlib.util
import struct

ROOT = Path(__file__).resolve().parents[3]


def score(events):
    return b"MUS\x1a" + struct.pack("<6H", len(events), 16, 2, 0, 0, 0) + events


def main():
    spec = importlib.util.spec_from_file_location(
        "music_importer", ROOT / "code/import-doom-music.py"
    )
    importer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(importer)
    midi = importer.convert(
        score(bytes((0x40, 0, 30, 0x90, 188, 100, 70, 0x0F, 35, 0x00, 60, 0x60)))
    )
    assert midi[:14] == b"MThd" + struct.pack(">IHHH", 6, 0, 1, 70)
    assert b"\x00\xc0\x1e" in midi
    assert b"\x00\x90\x3c\x64" in midi
    assert b"\x46\x89\x23\x40" in midi
    assert midi.endswith(b"\x00\xff\x2f\x00")
    defaults = importer.convert(score(bytes((0x40, 1, 2, 0x10, 60, 0x60))))
    assert b"\x00\xb0\x20\x02" in defaults
    assert b"\x00\x90\x3c\x7f" in defaults
    malformed = (
        b"",
        score(b"\x10"),
        score(b"\x50"),
        score(b"\x30\x00"),
        score(b"\x40\x0a\x00"),
        score(b"\x90\x3c\xff\xff\xff\xff"),
    )
    for data in malformed:
        try:
            importer.convert(data)
        except ValueError:
            pass
        else:
            raise AssertionError("Malformed MUS accepted")
    print("MUS conversion timing/channel/controller and malformed-input checks passed.")


if __name__ == "__main__":
    main()
