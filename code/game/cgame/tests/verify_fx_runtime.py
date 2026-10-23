"""Exercise native GLSL compilation, effect rendering, collision blood and weapon hits."""

import argparse
from pathlib import Path
import subprocess
import re

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    output = ROOT / "out" / f"vfx-runtime-{args.label}"
    base = output / "base"
    base.mkdir(parents=True, exist_ok=True)
    effects = (
        "pistol_wall",
        "pistol_slime",
        "shotgun_metal",
        "chaingun_glass",
        "rocket_wall",
        "plasma_wall",
        "bfg_wall",
        "pistol_flesh",
        "chainsaw_flesh",
    )
    commands = [
        "set com_maxfps 60",
        "set con_notifytime 0",
        "wait 150",
        "god",
        "set doom_ai_disable 1",
        "wait 10",
        "set r_hbao 0",
        "wait 4",
        "screenshot hbao_off",
        "set r_hbao 1",
        "set r_hbao_strength 60",
        "set r_hbao_radius 60",
        "wait 8",
        "hbaoinfo",
        "screenshot hbao_on",
        "wait 2",
        "doomfx status",
        "screenshot fx_baseline",
        "wait 2",
    ]
    for effect in effects:
        commands.extend(
            (
                "doomfx reload",
                "wait 2",
                f"doomfx {effect}",
                "wait 8",
                "doomfx status",
                f"screenshot fx_{effect}",
                "wait 80",
            )
        )
    commands.extend(
        (
            "doomfx reload",
            "wait 4",
            "doomfx blood_splat",
            "wait 8",
            "screenshot fx_blood_decal",
            "wait 15",
        )
    )
    commands.extend(
        (
            "give all",
            "weapon 2",
            "wait 35",
            "+attack",
            "wait 60",
            "-attack",
            "doomfx status",
            "screenshot fx_weapon_fire",
            "wait 10",
            "set r_customwidth 1040",
            "set r_customheight 585",
            "vid_restart",
            "wait 80",
            "hbaoinfo",
            "screenshot fx_odd_resize",
            "wait 4",
            "quit",
        )
    )
    (base / "verify.cfg").write_text("\n".join(commands) + "\n", encoding="ascii")
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
        ("r_fullscreen", 0),
        ("r_hiddenWindow", 1),
        ("r_mode", -1),
        ("r_customwidth", 1280),
        ("r_customheight", 720),
        ("r_gamma", 1),
        ("r_intensity", 1),
        ("r_overBrightBits", 0),
        ("r_smp", 0),
        ("r_vfxDebug", 1),
        ("in_mouse", 0),
        ("in_joystick", 0),
    ):
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", "e1m1", "+exec", "verify.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen(command, cwd=ROOT, startupinfo=startup)
    try:
        result = process.wait(timeout=55)
        if result:
            raise RuntimeError(f"Native FX process failed ({result}); see {base}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    log = (base / "doomconsole.log").read_text(errors="replace")
    if "72/72 weapon/material, blast and trail templates loaded" not in log:
        raise RuntimeError("Native FX registration did not finish")
    if not re.search(r"Screen decals: [1-9]\d* projected", log):
        raise RuntimeError("Native depth-projector pass did not draw decals")
    ambient_views = re.findall(
        r"HBAO: enabled=1,.*? (\d+)x(\d+) half-resolution buffer, (\d+) views", log
    )
    if len(ambient_views) < 2 or any(int(views) == 0 for _, _, views in ambient_views):
        raise RuntimeError("HBAO did not render before and after video restart")
    for program in ("hbao_evaluate.frag", "hbao_composite.frag"):
        if f"GLSL: loaded glprogs/{program}" not in log:
            raise RuntimeError(f"Missing native HBAO program: {program}")
    statuses = re.findall(r"Doom FX: templates=72 active=(\d+) drawn=(\d+)", log)
    if len(statuses) < len(effects) + 2 or any(
        int(active) == 0 or int(drawn) == 0 for active, drawn in statuses[1 : len(effects) + 1]
    ):
        raise RuntimeError("A native preview did not simulate and draw particles")
    for failure in (
        "GLSL: linking",
        "GLSL: cannot",
        "GLSL: missing",
        "GL_SelectTexture: unit",
        "GL_INVALID",
        "Unknown key",
        "Invalid effect",
    ):
        if failure in log:
            raise RuntimeError(f"Native FX error: {failure}; see {base}")
    baseline = Image.open(base / "screenshots/fx_baseline.tga").convert("RGB")
    ambient_off = Image.open(base / "screenshots/hbao_off.tga").convert("RGB")
    ambient_on = Image.open(base / "screenshots/hbao_on.tga").convert("RGB")
    darkened = sum(
        sum(on) < sum(off) - 6
        for off, on in zip(ambient_off.get_flattened_data(), ambient_on.get_flattened_data())
    )
    if darkened < 1000:
        raise RuntimeError(f"HBAO did not darken enough visible contact pixels: {darkened}")
    if ImageChops.difference(
        ambient_off.crop((0, 605, 1280, 720)), ambient_on.crop((0, 605, 1280, 720))
    ).getbbox():
        raise RuntimeError("HBAO changed the HUD")
    ambient_off.save(output / "hbao_off.png")
    ambient_on.save(output / "hbao_on.png")
    resized = Image.open(base / "screenshots/fx_odd_resize.tga")
    if resized.size != (1040, 585):
        raise RuntimeError(f"Odd-sized video restart failed: {resized.size}")
    resized.save(output / "odd_resize.png")
    Image.open(base / "screenshots/fx_blood_decal.tga").save(output / "blood_decal.png")
    for effect in effects:
        image = Image.open(base / f"screenshots/fx_{effect}.tga").convert("RGB")
        if image.size != (1280, 720):
            raise RuntimeError(f"Invalid capture dimensions: {effect}")
        if ImageChops.difference(image, baseline).getbbox() is None:
            raise RuntimeError(f"Effect capture is unchanged: {effect}")
        image.save(output / f"{effect}.png")
    print(
        f"Native FX/HBAO: GLSL, 72 registrations, {len(effects)} effects, blood decal, pistol fire, "
        f"{darkened} AO-darkened pixels, unchanged HUD and odd-size video restart passed: {output}"
    )


if __name__ == "__main__":
    main()
