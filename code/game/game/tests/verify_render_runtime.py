"""Exercise barrel sprites/chains, weapon placement and black console in the native renderer."""

import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import compiler_command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    home = ROOT / f"out/render-{args.label}"
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    points = [(0, 0), (0, 768), (1024, 768), (1024, 0)]
    lines = [
        f"({a[0]},{a[1]}) to ({b[0]},{b[1]}) : 1 : 0 : 0\n    0 (0 : STARTAN3 / STARTAN3 / STARTAN3 )\n    0 : FLOOR4_8 128 : F_SKY1 192 0 0\n"
        for a, b in zip(points, points[1:] + points[:1])
    ]
    source = home / "barrels.dwd"
    source.write_text(
        "WorldServer version 4\n\nlines:4\n"
        + "".join(lines)
        + "\nthings:5\n(250,384,0) :1, 7\n(400,384,0) :2035, 7\n(464,384,0) :2035, 7\n(800,640,0) :2035, 7\n(700,500,0) :2018, 7\n"
    )
    spec = importlib.util.spec_from_file_location(
        "helpers", Path(__file__).with_name("verify_monster_runtime.py")
    )
    helpers = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helpers)
    helpers.run(
        [*compiler_command("dmap"), "--base", base, "--wad", ROOT / "base/wad/DOOM.WAD", source],
        home / "doommap.log",
    )
    geometry_spec = importlib.util.spec_from_file_location(
        "geometry", ROOT / "code/tools/compilers/dmap/tests/verify.py"
    )
    geometry = importlib.util.module_from_spec(geometry_spec)
    geometry_spec.loader.exec_module(geometry)
    left, door, right = (
        geometry.sector(light=144),
        geometry.sector(ceiling=0, light=192),
        geometry.sector(light=224),
    )
    door_source = home / "door.dwd"
    geometry.fixture(
        door_source,
        [
            geometry.line((0, 0), (0, 128), left),
            geometry.line((0, 128), (128, 128), left),
            geometry.line((128, 0), (0, 0), left),
            geometry.line((128, 128), (128, 0), left, door, 1),
            geometry.line((128, 128), (160, 128), door),
            geometry.line((160, 0), (128, 0), door),
            geometry.line((160, 128), (160, 0), door, right),
            geometry.line((160, 128), (288, 128), right),
            geometry.line((288, 128), (288, 0), right),
            geometry.line((288, 0), (160, 0), right),
        ],
    )
    helpers.run(
        [
            *compiler_command("dmap"),
            "--base",
            base,
            "--wad",
            ROOT / "base/wad/DOOM.WAD",
            door_source,
        ],
        home / "door-doommap.log",
    )
    (base / "verify.cfg").write_text(
        "\n".join(
            (
                "set com_maxfps 125",
                "set con_notifytime 0",
                "wait 250",
                "god",
                "setviewpos 250 384 24.125 0",
                "wait 80",
                "screenshot barrels_before",
                "+attack",
                "wait 400",
                "-attack",
                "wait 100",
                "screenshot barrels_after",
                "spdevmap door",
                "wait 250",
                "set developer 1",
                "setviewpos 88 64 24.125 0",
                "wait 80",
                "+activate",
                "wait 30",
                "-activate",
                "wait 2000",
                "spdevmap e1m1",
                "wait 250",
                "set doom_ai_disable 1",
                "give all",
                "weaponbank 2",
                "wait 500",
                "screenshot pistol_position",
                "weaponbank 3",
                "wait 500",
                "screenshot shotgun_position",
                "s_musicinfo",
                "toggleconsole",
                "wait 90",
                "screenshot black_console",
                "quit",
                "",
            )
        )
    )
    command = [ROOT / "Doom.exe"]
    for name, value in (
        ("fs_basepath", ROOT),
        ("fs_homepath", home),
        ("vm_game", 0),
        ("vm_cgame", 0),
        ("vm_ui", 0),
        ("sv_pure", 0),
        ("logfile", 2),
        ("com_blindlyLoadDLLs", 1),
        ("r_fullscreen", 0),
        ("r_mode", 1),
        ("r_hiddenWindow", 1),
        ("in_mouse", 0),
        ("in_joystick", 0),
    ):
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", "barrels", "+exec", "verify.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(x) for x in command], cwd=ROOT, startupinfo=startup)
    try:
        result = process.wait(timeout=65)
        if result:
            raise RuntimeError(f"Native renderer failed: {result}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    text = (base / "doomconsole.log").read_text(errors="replace")
    if len(re.findall("Doom barrel exploded:", text)) != 2:
        raise RuntimeError("Shooting the first barrel did not chain to exactly its neighbor")
    if not re.search(
        r"Doom music: track=music/doom/d_e1m1.mid playing=1 queued=\d+ position=[1-9]", text
    ):
        raise RuntimeError("Level music stopped after initialization")
    if "Doom mover sound: doropn" not in text or "Doom mover sound: dorcls" not in text:
        raise RuntimeError("Opening and closing door sound events did not replicate")
    print(
        f"Native barrel damage/chaining, weapon and console capture, and sustained level music passed: {home}"
    )


if __name__ == "__main__":
    main()
