"""Load every campaign map and check native difficulty-filtered spawn counts."""

import argparse
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
MONSTERS = {3004, 9, 3001, 3002, 58, 3006, 3005, 3003, 16, 7}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--binaries", type=Path, help="Override native binary directory")
    parser.add_argument("--label", help="Distinct output label for another toolchain")
    args = parser.parse_args()
    binaries = args.binaries.resolve() if args.binaries else ROOT
    home = ROOT / f"out/monster-maps-{args.label or args.configuration.lower()}"
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    cases = [(path.stem, 3) for path in sorted((ROOT / "base/maps").glob("*.bsp"))]
    cases.extend(("e1m1", skill) for skill in range(5))
    commands = ["set com_maxfps 125", "set developer 1"]
    expected = []
    item_types = {
        record["thing"] for record in json.loads((ROOT / "base/editor/doom-items.json").read_text())
    }
    expected_items = []
    for name, skill in cases:
        data = (ROOT / f"base/maps/{name}.bsp").read_bytes()
        offset, length = struct.unpack_from("<ii", data, 8 + 21 * 8)
        things = list(struct.iter_unpack("<6i", data[offset : offset + length]))
        # Original P_SpawnMapThing: baby/easy use bit one, medium bit two,
        # hard/nightmare bit four. Bit sixteen excludes single-player things.
        bit = (1, 1, 2, 4, 4)[skill]
        count = sum(t[3] in MONSTERS and bool(t[4] & bit) and not t[4] & 16 for t in things)
        expected.append((name, skill, count))
        expected_items.append(
            (sum(t[3] in item_types and bool(t[4] & bit) and not t[4] & 16 for t in things), skill)
        )
        commands.extend(
            (f"set doom_skil
l {skill}", f"spdevmap {name}", "wait 180", "doomai nav", "doomai")
        )
    commands.append("quit")
    (base / "verify.cfg").write_text("\n".join(commands) + "\n")
    command = [binaries / "Doom.exe"]
    for name, value in (
        ("fs_basepath", ROOT),
        ("fs_homepath", home),
        ("dedicated", 2),
        ("sv_pure", 0),
        ("logfile", 2),
        ("vm_game", 0),
        ("com_blindlyLoadDLLs", 1),
        ("net_port", 27972),
    ):
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", "e1m1", "+exec", "verify.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(value) for value in command], cwd=binaries, startupinfo=startup)
    (home / "process-id.txt").write_text(str(process.pid))
    try:
        result = process.wait(timeout=600)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=10)
        raise RuntimeError("Native map verification timed out") from None
    if result:
        raise RuntimeError(
            f"Native map verification exited {result}; see {base / 'doomconsole.log'}"
        )
    text = (base / "doomconsole.log").read_text(errors="replace")
    records = [
        (name, int(skill), int(count))
        for name, count, skill in re.findall(
            r"Doom navigation: (\w+) result=0.*?Doom monsters: (\d+) spawn anchors, skill=(\d+)",
            text,
            re.S,
        )
    ]
    # The initial +spdevmap precedes the verification config.
    if records[-len(expected) :] != expected:
        raise RuntimeError(f"Map spawn verification differs: expected {expected}, found {records}")
    if "WARNING: No clear Doom spawn hull" in text:
        raise RuntimeError("A campaign monster still has an obstructed spawn hull")
    item_records = [
        (int(count), int(skill))
        for count, skill in re.findall(
            r"Doo
m items: spawned (\d+) original pickups at skill (\d+)", text
        )
    ]
    if item_records[-len(expected_items) :] != expected_items:
        raise RuntimeError(f"Native pickup spawn filtering differs: {item_records}")
    worlds = re.findall(r"Doom navigation: world=([0-3]) initialized=([01])", text)
    if len(worlds) != len(expected) * 4 or any(ready != "1" for _, ready in worlds):
        raise RuntimeError("Not every campaign navigation world initialized")
    (home / "verification.json").write_text(
        json.dumps(
            {"passed": True, "cases": expected, "navigation_world_checks": len(worlds)}, indent=2
        )
        + "\n"
    )
    print(
        f"Native {args.configuration}: 27 campaign maps, all five skills, {len(worlds)} navigation world checks passed."
    )


if __name__ == "__main__":
    main()
