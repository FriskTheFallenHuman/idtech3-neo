"""Restore a copy of an existing save against a different monster spawn roster."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

from PIL import Image
from verify_save_runtime import ROOT, launch
import verify_save_runtime


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--save-file", type=Path, required=True)
    parser.add_argument("--label", default="release")
    parser.add_argument("--binaries", type=Path, default=ROOT)
    args = parser.parse_args()
    verify_save_runtime.BINARIES = args.binaries.resolve()
    source = args.save_file.resolve()
    original_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    output = ROOT / "out" / f"save-clients-{args.label}"
    saves = output / "base/save"
    saves.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, saves / "copied.svg")
    log = launch(
        output,
        "save/copied.svg",
        [
            "wait 220",
            "doomai player",
            "doomai",
            "savegame copied_roundtrip",
            "wait 10",
            "loadgame copied_roundtrip",
            "wait 180",
            "doomai player",
            "doomai",
            "spdevmap e2m1",
            "wait 180",
            "loadgame copied_roundtrip",
            "wait 180",
            "doomai player",
            "doomai",
            "screenshot restored_save",
            "wait 5",
            "quit",
        ],
        skill=2,
    )
    positions = re.findall(r"Doom player: at=([\d. -]+) height=", log)
    counts = [int(value) for value in re.findall(r"Doom save restored: (\d+) monster", log)]
    actors = re.findall(r"Doom AI: (\d+) (\w+) health=(-?\d+)", log)
    if len(positions) != 3 or len(set(positions)) != 1:
        raise RuntimeError(f"Player did not survive all three restoration paths: {positions}")
    if len(counts) != 3 or len(set(counts)) != 1 or len(actors) != 3 * counts[0]:
        raise RuntimeError(f"Restored actor roster differs: {counts}; {len(actors)} actor records")
    count = counts[0]
    if actors[:count] != actors[count : 2 * count] or actors[:count] != actors[2 * count :]:
        raise RuntimeError("Saved monster species/health changed after reloading")
    if hashlib.sha256(source.read_bytes()).hexdigest() != original_hash:
        raise RuntimeError("Original save file was modified")
    with Image.open(output / "base/screenshots/restored_save.tga") as capture:
        capture.save(output / "restored_save.png")
    (output / "validation.json").write_text(
        json.dumps(
            {
                "source_sha256": original_hash,
                "restored_monster_counts": counts,
                "restored_player_positions": positions,
                "monster_species_and_health_preserved": True,
                "source_unchanged": True,
            },
            indent=2,
        )
        + "\n"
    )
    print(f"Existing save, same/cross-map reloads and {count} saved monster slots passed: {output}")


if __name__ == "__main__":
    main()
