"""Capture real rocket/plasma/BFG flight, wall blasts and their aftermath in a native room."""

import argparse
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import compiler_command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    parser.add_argument("--binaries", type=Path, default=ROOT)
    args = parser.parse_args()
    output = ROOT / "out" / f"projectile-vfx-{args.label}"
    base = output / "base"
    base.mkdir(parents=True, exist_ok=True)
    points = ((0, 0), (0, 1024), (2048, 1024), (2048, 0))
    lines = []
    for start, end in zip(points, points[1:] + points[:1]):
        lines.append(
            f"({start[0]},{start[1]}) to ({end[0]},{end[1]}) : 1 : 0 : 0\n"
            "    0 (0 : STARTAN3 / STARTAN3 / STARTAN3 )\n"
            "    0 : FLOOR4_8 128 : CEIL3_5 192 0 0\n"
        )
    source = output / "projectilefx.dwd"
    source.write_text(
        "WorldServer version 4\n\nlines:4\n" + "".join(lines) + "\nthings:1\n(256,512,0) :1, 7\n",
        encoding="ascii",
    )
    with (output / "map-build.log").open("w") as log:
        subprocess.run(
            [
                *compiler_command("dmap"),
                "--base",
                str(base),
                "--wad",
                str(ROOT / "base/wad/DOOM.WAD"),
                str(source),
            ],
            cwd=ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
    commands = ["set con_notifytime 0", "wait 150", "god", "give all", "wait 20"]
    names = []
    for name, slot, firing in (("rocket", 5, 50), ("plasma", 6, 20), ("bfg", 7, 140)):
        names.append(name)
        commands += [
            "setviewpos 256 512 25 0",
            "wait 15",
            f"weapon {slot}",
            "wait 180",
            f"screenshot baseline_{name}",
            "+attack",
            f"wait {firing}",
            "-attack",
            "wait 10",
            f"screenshot flight_{name}",
            "wait 400",
            "setviewpos 1700 512 25 0",
            "wait 15",
            "doomfx reload",
            "wait 4",
            f"screenshot contact_baseline_{name}",
            f"doomfx {name}_wall",
            "wait 10",
            f"screenshot blast_{name}",
            "wait 35",
            f"screenshot aftermath_{name}",
            "wait 150",
        ]
    commands += ["doomfx status", "quit"]
    (base / "verify.cfg").write_text("\n".join(commands) + "\n", encoding="ascii")
    command = [str(args.binaries.resolve() / "Doom.exe")]
    for key, value in {
        "fs_basepath": ROOT,
        "fs_homepath": output,
        "vm_game": 0,
        "vm_cgame": 0,
        "vm_ui": 0,
        "com_blindlyLoadDLLs": 1,
        "sv_pure": 0,
        "logfile": 2,
        "com_maxfps": 60,
        "doom_ai_disable": 1,
        "r_fullscreen": 0,
        "r_hiddenWindow": 1,
        "r_mode": -1,
        "r_customwidth": 1280,
        "r_customheight": 720,
        "r_gamma": 1,
        "r_intensity": 1,
        "r_overBrightBits": 0,
        "r_smp": 0,
        "in_mouse": 0,
        "in_joystick": 0,
        "net_port": 27972,
    }.items():
        command += ["+set", key, str(value)]
    command += ["+spdevmap", "projectilefx", "+exec", "verify.cfg"]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen(command, cwd=args.binaries.resolve(), startupinfo=startup)
    try:
        if process.wait(timeout=60):
            raise RuntimeError(f"Native projectile test failed: {base}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    log = (base / "doomconsole.log").read_text(errors="replace")
    if any(
        failure in log for failure in ("ERROR:", "Unknown key", "Invalid effect", "GLSL: cannot")
    ):
        raise RuntimeError(f"Native projectile rendering error: {base}")
    report = {}
    for name in names:
        captures = {}
        for phase in ("baseline", "flight", "contact_baseline", "blast", "aftermath"):
            with Image.open(base / f"screenshots/{phase}_{name}.tga") as capture:
                captures[phase] = capture.convert("RGB")
                captures[phase].save(output / f"{phase}_{name}.png")
        region = (0, 0, 1280, 450)  # Exclude first-person weapon and HUD animation.
        changes = {}
        for phase, reference in (("flight", "baseline"), ("blast", "contact_baseline")):
            difference = ImageChops.difference(captures[phase], captures[reference]).crop(region)
            changes[phase] = sum(max(pixel) > 15 for pixel in difference.get_flattened_data())
            if changes[phase] < 10:
                raise RuntimeError(f"No visible {phase} effect for {name}: {changes}")
        report[name] = changes
    (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Real rocket, plasma and BFG flight/blast captures passed: {output}")


if __name__ == "__main__":
    main()
