"""Check a lone deathmatch bot traverses E1M1 for equipment without combat or respawn teleports."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binaries", type=Path, default=ROOT)
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    binaries = args.binaries.resolve()
    home = ROOT / "out" / f"bot-navigation-{args.label}"
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    map_path = ROOT / "base/maps/e1m1.bsp"
    original = hashlib.sha256(map_path.read_bytes()).hexdigest()
    commands = ["wait 180", "god", "notarget", "botstatus"]
    for _ in range(6):
        commands += ["wait 1200", "botstatus"]
    commands += ["screenshot bot_navigation", "quit"]
    (base / "verify.cfg").write_text("\n".join(commands) + "\n", encoding="ascii")
    command = [str(binaries / "Doom.exe")]
    for key, value in {
        "fs_basepath": ROOT,
        "fs_homepath": home,
        "vm_game": 0,
        "vm_cgame": 0,
        "vm_ui": 0,
        "com_blindlyLoadDLLs": 1,
        "sv_pure": 0,
        "logfile": 2,
        "com_maxfps": 125,
        "r_fullscreen": 0,
        "r_hiddenWindow": 1,
        "r_mode": 3,
        "r_smp": 0,
        "in_mouse": 0,
        "net_port": 27976,
        "g_gametype": 0,
        "sv_maxclients": 16,
        "g_doWarmup": 0,
        "fraglimit": 0,
        "timelimit": 0,
        "doom_botCount": 1,
        "doom_botSkill": 3,
        "bot_enable": 1,
        "doom_skill": 2,
    }.items():
        command += ["+set", key, str(value)]
    command += ["+devmap", "e1m1", "+exec", "verify.cfg"]
    with (home / "process.log").open("w") as output:
        subprocess.run(
            command,
            cwd=binaries,
            stdout=output,
            stderr=subprocess.STDOUT,
            timeout=150,
            check=True,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
    text = (base / "doomconsole.log").read_text(errors="replace")
    positions = [
        tuple(map(float, row))
        for row in re.findall(
            r"Doom bot movement: client=1 origin=([-\d.]+),([-\d.]+),([-\d.]+)", text
        )
    ]
    states = [
        dict(re.findall(r"(\w+)=([^ ]+)", line))
        for line in text.splitlines()
        if line.startswith("Doom bot: ")
    ]
    if len(positions) < 7 or len(states) < 7:
        raise RuntimeError(f"Navigation samples missing: {home}")
    if any(int(state["spawn"]) != 1 for state in states):
        raise RuntimeError("Bot respawned, so location changes cannot establish traversal")
    extent = max(
        max(p[axis] for p in positions) - min(p[axis] for p in positions) for axis in range(2)
    )
    if extent < 600 or float(states[-1]["moved"]) < 1500:
        raise RuntimeError(
            f"Bot remained in one area: extent={extent}, states={states}, positions={positions}"
        )
    if not any(int(state["weapon"]) not in (32, 33) for state in states):
        raise RuntimeError(f"Bot failed to collect/use Doom map weapons: {states}")
    if not re.search(r"Doom deathmatch pickups: weapons=[3-9]", text):
        raise RuntimeError("E1M1 deathmatch weapon placement was not enabled")
    if hashlib.sha256(map_path.read_bytes()).hexdigest() != original:
        raise RuntimeError("Campaign BSP changed during verification")
    (home / "validation.json").write_text(
        json.dumps(
            {
                "positions": positions,
                "extent": extent,
                "states": states,
                "map_sha256": original,
                "passed": True,
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"E1M1 bot navigation and deathmatch pickups passed: {home}")


if __name__ == "__main__":
    main()
