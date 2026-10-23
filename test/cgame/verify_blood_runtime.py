"""Shoot a real monster near a wall in an isolated native blood-effects fixture."""

import argparse
from pathlib import Path
import re
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "code/tools/compilers"))
from integrated import compiler_command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    output = ROOT / "out" / f"blood-runtime-{args.label}"
    base = output / "base"
    base.mkdir(parents=True, exist_ok=True)
    source = output / "bloodroom.dwd"
    points = ((0, 0), (0, 384), (512, 384), (512, 0))
    lines = []
    for start, end in zip(points, points[1:] + points[:1]):
        lines.append(
            f"({start[0]},{start[1]}) to ({end[0]},{end[1]}) : 1 : 0 : 0\n"
            "    0 (0 : STARTAN3 / STARTAN3 / STARTAN3 )\n"
            "    0 : FLOOR4_8 128 : CEIL3_5 192 0 0\n"
        )
    source.write_text(
        "WorldServer version 4\n\nlines:4\n"
        + "".join(lines)
        + "\nthings:2\n(256,192,0) :1, 7\n(440,192,180) :3003, 7\n",
        encoding="ascii",
    )
    with (output / "map-build.log").open("w") as log:
        result = subprocess.run(
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
            check=False,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
    if result.returncode:
        raise RuntimeError(f"Blood fixture compilation failed: {output / 'map-build.log'}")
    (base / "verify.cfg").write_text(
        "set com_maxfps 60\nset con_notifytime 0\nwait 150\ngod\n"
        "version\ndoomai\ndoomfx status\nscreenshot blood_before\n"
        "+attack\nwait 
22\n-attack\nwait 2\n"
        "screenshot blood_exit\ndoomai\nwait 20\n"
        "setviewpos 300 96 25 25\nwait 30\nscreenshot blood_wall\nwait 5\nquit\n",
        encoding="ascii",
    )
    command = [str(ROOT / "Doom.exe")]
    for name, value in (
        ("fs_basepath", ROOT),
        ("fs_homepath", output),
        ("logfile", 2),
        ("vm_game", 0),
        ("vm_cgame", 0),
        ("vm_ui", 0),
        ("sv_pure", 0),
        ("com_blindlyLoadDLLs", 1),
        ("doom_ai_disable", 1),
        ("fx_debug", 1),
        ("r_vfxDebug", 1),
        ("r_fullscreen", 0),
        ("r_hiddenWindow", 1),
        ("r_mode", -1),
        ("r_customwidth", 1280),
        ("r_customheight", 720),
        ("r_gamma", 1),
        ("r_intensity", 1),
        ("r_overBrightBits", 0),
        ("in_mouse", 0),
        ("in_joystick", 0),
    ):
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", "bloodroom", "+exec", "verify.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen(command, cwd=ROOT, startupinfo=startup)
    try:
        result = process.wait(timeout=35)
        if result:
            raise RuntimeError(f"Native blood fixture failed ({result}): {base}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    log = (base / "doomconsole.log").read_text(errors="replace")
    if "Doom 1.00a" not in log:
        raise RuntimeError("Startup console did not report Doom 1.00a")
    exits = re.findall(
        r"Doom blood: weapon=\d+ victim=1 exit=([\d.-]+) ([\d.-]+) ([\d.-]+) wall=1", log
    )
    if not exits or not all(463 < float(x) < 466 and abs(float(y) - 192) < 1 for x, y, _ in exits):
        raise RuntimeError(f"Monster exit/back-wall effect did not follow shot direction: {exits}")
    health = re.findall(r"Doom AI: 1 monster_baronofhell health=(\d+)", log)
 
   if len(health) < 2 or int(health[-1]) >= int(health[0]):
        raise RuntimeError(f"Actual pistol damage was not exercised: {health}")
    if not re.search(r"Screen decals: [1-9]\d* projected", log):
        raise RuntimeError("Native blood projector was not drawn")
    for name in ("blood_before", "blood_exit", "blood_wall"):
        image = Image.open(base / f"screenshots/{name}.tga")
        if image.size != (1280, 720):
            raise RuntimeError(f"Incorrect blood capture size: {image.size}")
        image.save(output / f"{name}.png")
    print(
        f"Native blood: real pistol damage, exit spray, nearby wall decal and Doom 1.00a passed: {output}"
    )


if __name__ == "__main__":
    main()
