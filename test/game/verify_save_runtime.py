"""Verify E1M5 loading, same/cross-map saves and invalid callbacks in isolated native runs."""

import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import compiler_command

BINARIES = ROOT


def launch(home, map_name, commands, *, allow_error=False, skill=3):
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    (base / "verify.cfg").write_text("\n".join(commands) + "\n", encoding="ascii")
    command = [str(BINARIES / "Doom.exe")]
    for name, value in {
        "fs_basepath": ROOT,
        "fs_homepath": home,
        "logfile": 2,
        "developer": 1,
        "vm_game": 0,
        "vm_cgame": 0,
        "vm_ui": 0,
        "sv_pure": 0,
        "com_blindlyLoadDLLs": 1,
        "doom_ai_disable": 1,
        "doom_skill": skill,
        "com_maxfps": 60,
        "r_fullscreen": 0,
        "r_hiddenWindow": 1,
        "r_mode": -1,
        "r_customwidth": 1280,
        "r_customheight": 720,
        "in_mouse": 0,
        "in_joystick": 0,
        "net_port": 27971,
    }.items():
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", map_name, "+exec", "verify.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen(command, cwd=BINARIES, startupinfo=startup)
    try:
        result = process.wait(timeout=60)
        if result:
            raise RuntimeError(f"Native save test exited {result}: {home}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    log = (base / "doomconsole.log").read_text(errors="replace")
    if not allow_error and "ERROR:" in log:
        raise RuntimeError(f"Native save test reported an engine error: {home}")
    return log


def
 main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    parser.add_argument("--binaries", type=Path, default=ROOT)
    args = parser.parse_args()
    global BINARIES
    BINARIES = args.binaries.resolve()
    output = ROOT / "out" / f"save-runtime-{args.label}"
    log = launch(
        output,
        "e1m5",
        [
            "wait 180",
            "god",
            "doomai player",
            "doomai 1 hurt",
            "doomai",
            "savegame callback_e1m5",
            "wait 10",
            "doomai 1 kill",
            "setviewpos 0 0 200 90",
            "wait 5",
            "set doom_skill 0",
            "loadgame callback_e1m5",
            "wait 180",
            "doomai player",
            "doomai",
            "screenshot e1m5_loaded",
            "savegame callback_resaved",
            "wait 10",
            "spdevmap e1m1",
            "wait 180",
            "loadgame callback_e1m5",
            "wait 180",
            "doomai player",
            "doomai",
            "screenshot e1m5_cross_loaded",
            "wait 5",
            "quit",
        ],
    )
    positions = re.findall(r"Doom player: at=([\d. -]+) height=", log)
    health = re.findall(r"Doom AI: 1 monster_imp health=(\d+)", log)
    if len(positions) != 3 or len(set(positions)) != 1 or health != ["55"] * 3:
        raise RuntimeError(f"Saved player/monster state differs: {positions}, {health}")
    if log.count("G_LoadGame '(null)'") != 2 or log.count("Doom map ready: e1m5 ") != 3:
        raise RuntimeError("Both same-map and cross-map restoration must complete")
    data = (output / "base/save/callback_e1m5.svg").read_bytes()
    required = ["DoomReached", "DoomLineUse", "DoomLineTouch", "DoomItemThink", "DoomBarrelDie"]
    if struct.unpack_from("<i", data)[0] != 22 or any(
        name.encode() + b"\0" not in data for name in required
    ):
        raise RuntimeError("Legacy save version/name
s were not retained")
    if "Doom save callbacks: 1250 registered" not in log:
        raise RuntimeError("Native game DLL did not validate the migrated registry")
    for name in ("e1m5_loaded", "e1m5_cross_loaded"):
        with Image.open(output / f"base/screenshots/{name}.tga") as capture:
            capture.save(output / f"{name}.png")

    # Reproduce the former writer's zero-length/four-byte empty-fog record.
    # The enclosing format and all callback/entity bytes remain unchanged.
    fog_offset = 84 + struct.unpack_from("<i", data, 80)[0] + 36 + 64
    fog_length = struct.unpack_from("<i", data, fog_offset)[0]
    legacy_data = (
        data[:fog_offset] + struct.pack("<i", 0) + b"none" + data[fog_offset + 4 + fog_length :]
    )
    legacy_home = output / "legacy-fog"
    legacy_saves = legacy_home / "base/save"
    legacy_saves.mkdir(parents=True, exist_ok=True)
    (legacy_saves / "legacy.svg").write_bytes(legacy_data)
    legacy_log = launch(
        legacy_home, "save/legacy.svg", ["wait 180", "doomai player", "doomai", "quit"]
    )
    if (
        positions[0] not in legacy_log
        or "Doom AI: 1 monster_imp health=55" not in legacy_log
        or "G_LoadGame" not in legacy_log
    ):
        raise RuntimeError("Legacy empty-fog save did not restore its state")

    # Save a rocket in flight, let it damage a distant actor, then load it and
    # verify both the restored actor and the missile's eventual impact.
    rocket_home = output / "projectile"
    rocket_base = rocket_home / "base"
    rocket_base.mkdir(parents=True, exist_ok=True)
    source = rocket_home / "saverocket.dwd"
    points = ((0, 0), (0, 512), (4096, 512), (4096, 0))
    lines = []
    for start, end in zip(points, points[1:] + points[:1]):
        lines.append(
            f"({start[0]},{start[1]}) to ({end[0]},{end[1]}) : 1 : 0 : 0\n"
            "    0 (0 : STARTAN3 / STARTAN3 / STARTAN3 )\n"
            "    0 : FLOOR4_8 128 : CEIL3_5 192 0 0\n"
        )
    source.wr
ite_text(
        "WorldServer version 4\n\nlines:4\n"
        + "".join(lines)
        + "\nthings:2\n(256,256,0) :1, 7\n(3840,256,180) :3003, 7\n",
        encoding="ascii",
    )
    with (rocket_home / "map-build.log").open("w") as build_log:
        subprocess.run(
            [
                *compiler_command("dmap"),
                "--base",
                str(rocket_base),
                "--wad",
                str(ROOT / "base/wad/DOOM.WAD"),
                str(source),
            ],
            cwd=ROOT,
            stdout=build_log,
            stderr=subprocess.STDOUT,
            check=True,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
    rocket_log = launch(
        rocket_home,
        "saverocket",
        [
            "wait 150",
            "god",
            "give all",
            "wait 10",
            "weapon 5",
            "wait 160",
            "+attack",
            "wait 22",
            "-attack",
            "wait 12",
            "savegame callback_rocket",
            "wait 900",
            "doomai",
            "loadgame callback_rocket",
            "wait 150",
            "doomai",
            "savegame callback_rocket_restored",
            "wait 900",
            "doomai",
            "quit",
        ],
    )
    rocket_health = [
        int(value)
        for value in re.findall(r"Doom AI: 1 monster_baronofhell health=(\d+)", rocket_log)
    ]
    if len(rocket_health) != 3 or not (
        rocket_health[0] < 1000 == rocket_health[1] and rocket_health[0] == rocket_health[2]
    ):
        raise RuntimeError(f"Restored missile impact/actor health differs: {rocket_health}")
    for name in ("callback_rocket", "callback_rocket_restored"):
        if b"G_ExplodeMissile\0" not in (rocket_base / f"save/{name}.svg").read_bytes():
            raise RuntimeError(f"Live projectile callback missing from {name}")

    # Alter only callback bytes, keeping the original routing header and record
    # lengths. E
rrors must be rejected by the real game loader, without a crash.
    invalid_results = {}
    needle = b"DoomItemThink\0"
    for label, replacement, expected in (
        ("unknown", b"XoomItemThink\0", "ReadField: unknown function 'XoomItemThink'"),
        ("unterminated", b"DoomItemThinkX", "ReadField: unterminated save callback name"),
    ):
        home = output / label
        saves = home / "base/save"
        saves.mkdir(parents=True, exist_ok=True)
        (saves / "invalid.svg").write_bytes(data.replace(needle, replacement, 1))
        invalid_log = launch(home, "save/invalid.svg", ["wait 180", "quit"], allow_error=True)
        if expected not in invalid_log:
            raise RuntimeError(f"Malformed callback was not rejected: {home}")
        invalid_results[label] = expected

    report = {
        "map": "e1m5",
        "save_version": 22,
        "registered_names": 1250,
        "same_map_load_at_different_difficulty": "passed",
        "restored_positions": positions,
        "restored_monster_health": health,
        "saved_callbacks": required,
        "projectile_actor_health": rocket_health,
        "legacy_empty_fog": "loaded",
        "invalid_callbacks": invalid_results,
    }
    (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"E1M5, same/cross-map save/load and malformed callback checks passed: {output}")


if __name__ == "__main__":
    main()
