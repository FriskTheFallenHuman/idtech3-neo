"""Build a native pursuit fixture and verify original chase through real AAS.

Runs a hidden project-root game with isolated home data, using spdevmap. The installed WAD,
campaign maps, user saves and running game are unchanged. Diagnostics stay in out.
"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import compiler_command


def run(command, log, cwd=ROOT, timeout=180):
    with log.open("w") as output:
        result = subprocess.run(
            [str(value) for value in command],
            cwd=cwd,
            stdout=output,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
        )
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): see {log}")


def fixture(path):
    outside = [(0, 0), (0, 768), (1024, 768), (1024, 0)]
    pillar = [(384, 256), (512, 256), (512, 512), (384, 512)]
    lines = []
    for loop in (outside, pillar):
        for start, end in zip(loop, loop[1:] + loop[:1]):
            lines.append(
                f"({start[0]},{start[1]}) to ({end[0]},{end[1]}) : 1 : 0 : 0\n"
                "    0 (0 : STARTAN3 / STARTAN3 / STARTAN3 )\n"
                "    0 : FLOOR4_8 128 : CEIL3_5 192 0 0\n"
            )
    path.write_text(
        "WorldServer version 4\n\nlines:8\n"
        + "".join(lines)
        + "\nthings:4\n(896,384,180) :1, 7\n(128,384,0) :3002, 7\n"
        "(128,128,0) :3004, 7\n(128,640,0) :3001, 7\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    parser.add_argument("--binaries", type=Path, help="Override native binary directory")
    parser.add_argument("--label", help="Distinct output label
 for another toolchain")
    args = parser.parse_args()
    binaries = args.binaries.resolve() if args.binaries else ROOT
    home = ROOT / f"out/monster-pursuit-{args.label or args.configuration.lower()}"
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    source = home / "chase.dwd"
    fixture(source)
    run(
        [*compiler_command("dmap"), "--base", base, "--wad", ROOT / "base/wad/DOOM.WAD", source],
        home / "dmap.log",
    )
    for world, profile in enumerate(("small", "medium", "cyber", "spider")):
        run(
            [
                *compiler_command("aasbuild"),
                "--profile",
                profile,
                "--output",
                base / "maps",
                base / "maps/chase.bsp",
            ],
            home / f"aasbuild-{world}.log",
            home,
        )
    (base / "verify.cfg").write_text(
        "set com_maxfps 125\nset developer 1\nset r_clear 0\nset r_novis 0\nwait 300\ngod\ndoomai nav\n"
        "doomai 1 route\ndoomai 1 wake\ndoomai\nwait 500\ndoomai\nwait 500\ndoomai\n"
        "wait 500\ndoomai\ndoomai nav\nscreenshot monsters_pursuit\n"
        "savegame monster_pursuit\nwait 50\nloadgame monster_pursuit\nwait 400\n"
        "doomai\nspdevmap e1m1\nwait 300\nset doom_ai_disable 1\n"
        "setviewpos 400 -3232 25 180\nwait 150\nscreenshot monsters_armor_pillars\n"
        "+forward\nwait 300\n-forward\nwait 50\ndoomai player\n"
        "screenshot monsters_stairs\nquit\n"
    )
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
        ("r_textureMode", "GL_NEAREST"),
        ("com_blindlyLoadDLLs", 1),
        ("doom_skill", 2),
        ("net_port", 27970),
    ):
        command.extend(("+set", name, str(value)))
    c
ommand.extend(("+spdevmap", "chase", "+exec", "verify.cfg"))
    # STARTF_USESHOWWINDOW also hides the native window when the engine starts.
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(value) for value in command], cwd=binaries, startupinfo=startup)
    (home / "process-id.txt").write_text(str(process.pid))
    try:
        result = process.wait(timeout=180)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=10)
        raise RuntimeError(f"Native test timed out; see {base / 'doomconsole.log'}") from None
    if result:
        raise RuntimeError(f"Native test exited {result}; see {base / 'doomconsole.log'}")
    text = (base / "doomconsole.log").read_text(errors="replace")
    positions = [
        tuple(map(float, match))
        for match in re.findall(
            r"Doom AI: 1 monster_demon .*?at=([-\d.]+) ([-\d.]+) ([-\d.]+)", text
        )
    ]
    if len(positions) < 4 or max(position[0] for position in positions) < 700:
        raise RuntimeError(f"Demon failed pursuit around pillar: {positions}")
    if not re.search(r"Doom navigation: world=1 initialized=1 queries=\d+ routes=[1-9]", text):
        raise RuntimeError("Native pursuit did not use successful AAS routing")
    if "Game loaded" not in text and "G_LoadGame" not in text:
        raise RuntimeError("Save/load was not exercised")
    player = re.search(r"Doom player: at=([-\d.]+) ([-\d.]+) ([-\d.]+)", text)
    if not player or float(player[1]) > 180 or float(player[3]) < 100:
        raise RuntimeError(f"Player failed the original E1M1 staircase: {player}")
    if "2 tech pillars" not in text:
        raise RuntimeError("The original E1M1 tech pillars did not spawn")
    (home / "verification.json").write_text(
        json.dumps({"positions": positions, "passed": True}, indent=2)
    )
    print(f"Native {args.configuration} pursuit, AAS rou
ting and save/load passed: {home}")


if __name__ == "__main__":
    main()
