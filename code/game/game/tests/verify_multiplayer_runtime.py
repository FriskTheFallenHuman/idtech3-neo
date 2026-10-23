"""Run Doom deathmatch/team bots, item respawns and roster reloads in a native arena."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import compiler_command

BINARIES = ROOT


def run(command, log, cwd=ROOT, timeout=180):
    with log.open("w") as output:
        subprocess.run(
            list(map(str, command)),
            cwd=cwd,
            stdout=output,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=True,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )


def fixture(output):
    base = output / "base"
    base.mkdir(parents=True, exist_ok=True)
    outside = [(0, 0), (0, 1024), (1536, 1024), (1536, 0)]
    pillar = [(640, 384), (896, 384), (896, 640), (640, 640)]
    lines = []
    for loop in (outside, pillar):
        for start, end in zip(loop, loop[1:] + loop[:1]):
            lines.append(
                f"({start[0]},{start[1]}) to ({end[0]},{end[1]}) : 1 : 0 : 0\n"
                "    0 (0 : STARTAN3 / STARTAN3 / STARTAN3 )\n"
                "    0 : FLOOR4_8 64 : CEIL3_5 192 0 0\n"
            )
    things = [
        (768, 160, 90, 1),
        (160, 160, 0, 11),
        (1376, 160, 180, 11),
        (160, 864, 0, 11),
        (1376, 864, 180, 11),
        (768, 864, 270, 11),
        (256, 160, 0, 2001),
        (1280, 160, 0, 2002),
        (256, 864, 0, 2004),
        (1280, 864, 0, 2003),
        (768, 768, 0, 2006),
        (768, 256, 0, 2012),
        (384, 512, 0, 2019),
        (1152, 512, 0, 17),
        (500, 160, 0, 2049),
        (500, 864, 0, 2046),
        (1120, 864, 0, 2048),
        (1100, 160, 0, 3004),
        (1000, 256, 0, 2005),
        (768, 200, 0, 2013),
    ]
    source = output / "doombotarena.dwd"
    source.write_text(
        "WorldServer version 4\n\nlines:8\n"
        + "".join(lines)
        + f"\nthings:{len(things)}\n"
        + "".join(
            f"({x},{y},{angle}) :{kind}, {23 if kind == 2005 else 7}\n"
            for x, y, angle, kind in things
        ),
        encoding="ascii",
    )
    run(
        [*compiler_command("dmap"), "--base", base, "--wad", ROOT / "base/wad/DOOM.WAD", source],
        output / "dmap.log",
    )
    for world, profile in enumerate(("small", "medium", "cyber", "spider")):
        run(
            [
                *compiler_command("aasbuild"),
                "--profile",
                profile,
                "--output",
                base / "maps",
                base / "maps/doombotarena.bsp",
            ],
            output / f"aasbuild-{world}.log",
            output,
        )
    return base


def native(output, mode, label):
    home = output / mode
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    commands = [
        "wait 180",
        "god",
        "notarget",
        "botstatus",
        "wait 4000",
        "botstatus",
        "doomai",
        "+scores",
        "wait 20",
        f"screenshot bots_{mode}",
        "-scores",
        "map_restart 0",
        "wait 450",
        "botstatus",
        "removebot all",
        "wait 60",
        "botstatus",
        "quit",
    ]
    (base / "verify.cfg").write_text("\n".join(commands) + "\n", encoding="ascii")
    command = [BINARIES / "Doom.exe"]
    values = {
        "fs_basepath": ROOT,
        "fs_homepath": home,
        "vm_game": 0,
        "vm_cgame": 0,
        "vm_ui": 0,
        "com_blindlyLoadDLLs": 1,
        "sv_pure": 0,
        "logfile": 2,
        "developer": 1,
        "com_maxfps": 125,
        "r_fullscreen": 0,
        "r_hiddenWindow": 1,
        "r_mode": 3,
        "r_smp": 0,
        "in_mouse": 0,
        "net_port": 27975,
        "g_gametype": 3 if mode == "team" else 0,
        "sv_maxclients": 16,
        "g_doWarmup": 0,
        "g_warmup": 0,
        "fraglimit": 0,
        "timelimit": 0,
        "g_friendlyFire": 0,
        "doom_botCount": 4,
        "doom_botSkill": 5,
        "name": "DoomTester",
        "doom_skill": 2,
        "bot_enable": 1,
    }
    # The fixture lives in a separate read path; profiles and user configuration stay isolated.
    values["fs_basepath"] = ROOT
    for key, value in values.items():
        command.extend(("+set", key, str(value)))
    command.extend(("+devmap", "doombotarena", "+exec", "verify.cfg"))
    import shutil

    for directory in ("maps", "scripts", "textures"):
        shutil.copytree(output / "base" / directory, base / directory, dirs_exist_ok=True)
    (base / "games.log").write_text("", encoding="ascii")
    run(command, home / "process.log", timeout=120)
    text = (base / "doomconsole.log").read_text(errors="replace")
    if "0 spawn anchors" not in text or "Doom bot roster: 0" not in text:
        raise RuntimeError(f"Monsters spawned or bot removal failed: {home}")
    if "Doom deathmatch pickups: weapons=6" not in text:
        raise RuntimeError(f"Deathmatch-only weapons were not spawned: {home}")
    rows = [
        dict(re.findall(r"(\w+)=([^ ]+)", line))
        for line in text.splitlines()
        if line.startswith("Doom bot: ")
    ]
    if len(rows) < 12:
        raise RuntimeError(f"Bots did not spawn/reload: {home}")
    played = rows[4:8]
    for row in played:
        if int(row["commands"]) < 100 or float(row["moved"]) < 100 or int(row["attacks"]) < 1:
            raise RuntimeError(f"Bot failed movement/combat: {row}")
    if not any(int(row["routes"].split("/")[0]) > 0 for row in played):
        raise RuntimeError("No successful AAS routes")
    game_log = (base / "games.log").read_text(errors="replace")
    if not any(int(row["spawn"]) > 1 for row in played) or "Kill:" not in game_log:
        raise RuntimeError(f"Bot damage, death and respawn were not exercised: {home}")
    if mode == "team" and {int(row["team"]) for row in played} != {1, 2}:
        raise RuntimeError(f"Team bots were not assigned both teams: {played}")
    for path in (base / "screenshots").glob("*.tga"):
        with Image.open(path) as picture:
            picture.save(home / f"{path.stem}.png")
    return {"mode": mode, "label": label, "bots": played, "passed": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    parser.add_argument("--mode", choices=("deathmatch", "team", "both"), default="both")
    parser.add_argument("--binaries", type=Path, default=ROOT)
    args = parser.parse_args()
    global BINARIES
    BINARIES = args.binaries.resolve()
    output = ROOT / "out" / f"multiplayer-{args.label}"
    fixture(output)
    results = [
        native(output, mode, args.label)
        for mode in (("deathmatch", "team") if args.mode == "both" else (args.mode,))
    ]
    (output / "validation.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    print(f"Native Doom multiplayer checks passed: {output}")


if __name__ == "__main__":
    main()
