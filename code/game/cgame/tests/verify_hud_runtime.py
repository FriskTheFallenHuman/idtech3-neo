"""Capture the native widescreen HUD and compare its margins to the original flat."""

import argparse
from pathlib import Path
import subprocess

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    home = ROOT / f"out/hud-{args.label}"
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    (base / "verify.cfg").write_text(
        "set com_maxfps 125\nset con_notifytime 0\nwait 250\n"
        "god\nset doom_ai_disable 1\nwait 80\nscreenshot hud_sides\nwait 10\nquit\n"
    )
    command = [ROOT / "Doom.exe"]
    for name, value in (
        ("fs_basepath", ROOT),
        ("fs_homepath", home),
        ("logfile", 2),
        ("vm_game", 0),
        ("vm_cgame", 0),
        ("vm_ui", 0),
        ("sv_pure", 0),
        ("com_blindlyLoadDLLs", 1),
        ("r_fullscreen", 0),
        ("r_hiddenWindow", 1),
        ("r_mode", -1),
        ("r_customwidth", 1280),
        ("r_customheight", 720),
        ("r_gamma", 1),
        ("r_intensity", 1),
        ("r_overBrightBits", 0),
        ("r_textureMode", "GL_NEAREST"),
        ("in_mouse", 0),
        ("in_joystick", 0),
    ):
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", "e1m1", "+exec", "verify.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(item) for item in command], cwd=ROOT, startupinfo=startup)
    try:
        result = process.wait(timeout=45)
        if result:
            raise RuntimeError(
                f"Native HUD test failed with {result}; see {base / 'doomconsole.log'}"
            )
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    with Image.open(base / "screenshots/hud_sides.tga") as captured:
        screen = captured.convert("RGB")
    if screen.size != (1280, 720):
        raise RuntimeError(f"HUD capture dimensions differ: {screen.size}")
    with Image.open(ROOT / "base/ui/doom/floor7_2.tga") as texture:
        tile = texture.convert("RGB")
    samples = 0
    for y in (630, 660, 690, 710):
        for x in (*range(5, 155, 7), *range(1125, 1275, 7)):
            expected = tile.getpixel((int((x + 0.5) / 3) % 64, int((y + 0.5) / 3.6) % 64))
            actual = screen.getpixel((x, y))
            if max(abs(a - b) for a, b in zip(actual, expected)) > 1:
                raise RuntimeError(f"HUD border pixel differs at {(x, y)}: {actual} != {expected}")
            samples += 1
    screen.save(home / "hud.png")
    print(f"Native widescreen HUD passed {samples} original-flat pixel checks: {home / 'hud.png'}")


if __name__ == "__main__":
    main()
