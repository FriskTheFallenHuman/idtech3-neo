"""Touch every original pickup in an isolated native SP fixture and restore its save."""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import compiler_command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--binaries", type=Path, help="Override native binary directory")
    parser.add_argument("--label", help="Distinct output label for another toolchain")
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location(
        "runtime_helpers", Path(__file__).with_name("verify_monster_runtime.py")
    )
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    binaries = args.binaries.resolve() if args.binaries else ROOT
    home = ROOT / f"out/items-{args.label or args.configuration.lower()}"
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    definitions = json.loads((ROOT / "base/editor/doom-items.json").read_text())
    loop = [(0, 0), (0, 1536), (2048, 1536), (2048, 0)]
    lines = [
        f"({a[0]},{a[1]}) to ({b[0]},{b[1]}) : 1 : 0 : 0\n"
        "    0 (0 : STARTAN3 / STARTAN3 / STARTAN3 )\n"
        "    0 : FLOOR4_8 128 : CEIL3_5 96 0 0\n"
        for a, b in zip(loop, loop[1:] + loop[:1])
    ]
    things = ["(1900,1400,180) :1, 7"]
    positions = []
    for i, item in enumerate(definitions):
        x, y = 128 + i % 8 * 192, 128 + i // 8 * 256
        positions.append((x, y))
        things.append(f"({x},{y},0) :{item['thing']}, 7")
    source = home / "items.dwd"
    source.write_text(
        "WorldServer version 4\n\nlines:4\n"
        + "".join(lines)
        + f"\nthings:{len(things)}\n"
        + "\n".join(things)
        + "\n"
    )
    helpers.run(
        [*compiler_command("dmap"), "--base", base, "--wad", ROOT / "base/wad/DOOM.WAD", source],
        home / "doommap.log",
    )
    hazard = home / "hazard.dwd"
    hazard.write_text(
        "WorldServer version 4\n\nlines:4\n"
        + "".join(line.replace("CEIL3_5 96 0 0", "CEIL3_5 96 5 0") for line in lines)
        + "\nthings:2\n(1900,1400,180) :1, 7\n(128,128,0) :2025, 7\n"
    )
    helpers.run(
        [*compiler_command("dmap"), "--base", base, "--wad", ROOT / "base/wad/DOOM.WAD", hazard],
        home / "hazard-doommap.log",
    )
    commands = [
        "set com_maxfps 125",
        "set developer 1",
        "wait 200",
        "doomai player",
        "screenshot items_sector_visibility",
        "wait 20",
        "give health -50",
        "wait 20",
    ]
    for x, y in positions:
        commands.extend((f"setviewpos {x} {y} 24.125 180", "wait 40", "doomai player"))
    commands.extend(
        (
            "setviewpos 1900 1400 24.125 180",
            "wait 40",
            "doomvis",
            "screenshot items_fullbright",
            "wait 20",
            "+scores",
            "wait 40",
            "screenshot items_automap",
            "wait 20",
            "-scores",
            "savegame items",
            "wait 40",
            "loadgame items",
            "wait 200",
            "doomai player",
            "spdevmap hazard",
            "wait 250",
            "doomai player",
            "setviewpos 128 128 24.125 180",
            "wait 40",
            "doomai player",
            "wait 500",
            "doomai player",
            "quit",
        )
    )
    (base / "verify.cfg").write_text("\n".join(commands) + "\n")
    command = [binaries / "Doom.exe"]
    for name, value in (
        ("fs_basepath", ROOT),
        ("fs_homepath", home),
        ("vm_game", 0),
        ("vm_cgame", 0),
        ("vm_ui", 0),
        ("sv_pure", 0),
        ("logfile", 2),
        ("r_fullscreen", 0),
        ("r_mode", 3),
        ("com_blindlyLoadDLLs", 1),
        ("doom_skill", 2),
        ("net_port", 27974),
    ):
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", "items", "+exec", "verify.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(value) for value in command], cwd=binaries, startupinfo=startup)
    (home / "process-id.txt").write_text(str(process.pid))
    try:
        result = process.wait(timeout=120)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=10)
        raise RuntimeError(f"Native item test timed out: {base / 'doomconsole.log'}") from None
    if result:
        raise RuntimeError(f"Native item test exited {result}: {base / 'doomconsole.log'}")
    text = (base / "doomconsole.log").read_text(errors="replace")
    records = [
        tuple(map(int, record))
        for record in re.findall(
            r"Doom inventory: health=(\d+) armor=(\d+) type=(\d+) keys=(\d+) berserk=(\d+) map=(\d+) items=(\d+) remaining=(\d+) powers=(\d+),(\d+),(\d+),(\d+)",
            text,
        )
    ]
    if len(records) != 39 or [record[7] for record in records[:35]] != list(range(34, -1, -1)):
        raise RuntimeError(f"Physical pickup sequence failed: {records}")
    if records[35][:8] != (186, 200, 2, 126, 1, 1, 9, 0) or not all(records[35][8:]):
        raise RuntimeError(f"Saved inventory/power durations differ: {records[35]}")
    if not (0 < records[36][0] < 100) or records[37][0] != records[38][0] or not records[38][10]:
        raise RuntimeError(f"Radiation suit did not stop damaging-sector ticks: {records[36:]}")
    if not re.search(r"fogged=0 fullbright=1", text):
        raise RuntimeError("Visor/invulnerability did not disable distance darkness")
    (home / "verification.json").write_text(
        json.dumps(
            {
                "passed": True,
                "pickups": 34,
                "inventory": records[35],
                "hazard_health": [r[0] for r in records[36:]],
            },
            indent=2,
        )
    )
    print(
        f"Native {args.configuration}: all 34 physical pickups, sector hazards, fullbright visibility, client automap and inventory/power save restoration passed."
    )


if __name__ == "__main__":
    main()
