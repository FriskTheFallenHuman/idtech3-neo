"""Verify registered/demo filesystem detection and map/save gates in isolated native runs."""

import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BINARIES = ROOT


def launch(home, install, commands, start_map, *, demo):
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    (base / "verify.cfg").write_text("\n".join(commands) + "\n", encoding="ascii")
    command = [str(BINARIES / "Doom.exe")]
    settings = {
        "fs_basepath": install,
        "fs_homepath": home,
        "logfile": 2,
        "developer": 1,
        "vm_game": 0,
        "vm_cgame": 0,
        "vm_ui": 0,
        "sv_pure": 0,
        "com_blindlyLoadDLLs": 1,
        "doom_ai_disable": 1,
        "doom_demo": 0 if demo else 1,
        "com_maxfps": 125,
        "r_fullscreen": 0,
        "r_hiddenWindow": 1,
        "r_mode": -1,
        "r_customwidth": 640,
        "r_customheight": 480,
        "in_mouse": 0,
        "in_joystick": 0,
        "net_port": 27989,
    }
    for name, value in settings.items():
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", start_map, "+exec", "verify.cfg"))
    process = subprocess.Popen(
        command,
        cwd=install,
        creationflags=subprocess.CREATE_NO_WINDOW,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )
    try:
        if process.wait(timeout=60):
            raise RuntimeError(f"Native content run failed: {home}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    log = (base / "doomconsole.log").read_text(errors="replace")
    if "ERROR:" in log:
        raise RuntimeError(f"Native content run reported an engine error: {home}")
    return log


def demo_install(destination):
    """Mount existing assets without mounting the user's WAD or mutable directories."""
    base = destination / "base"
    base.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / "base").iterdir():
        target = base / source.name
        if source.is_dir() and source.name.lower() not in {"wad", "save", "screenshots"}:
            if not target.exists():
                # Directory junctions do not require Windows symlink privileges.
                quoted_target = str(target).replace("'", "''")
                quoted_source = str(source).replace("'", "''")
                subprocess.run(
                    [
                        "powershell.exe",
                        "-NoProfile",
                        "-Command",
                        "$ErrorActionPreference='Stop'; "
                        f"New-Item -ItemType Junction -Path '{quoted_target}' "
                        f"-Target '{quoted_source}' | Out-Null",
                    ],
                    check=True,
                    creationflags=subprocess.CREATE_NO_WINDOW,
                )
        elif source.is_file() and (
            source.suffix.lower() in {".dll", ".pk3"} or source.name == "default.cfg"
        ):
            binary_source = BINARIES / "base" / source.name if source.suffix == ".dll" else source
            shutil.copy2(binary_source, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    parser.add_argument("--binaries", type=Path, default=ROOT)
    args = parser.parse_args()
    global BINARIES
    BINARIES = args.binaries.resolve()
    output = ROOT / "out" / f"content-runtime-{args.label}"
    registered = output / "registered"
    full_install = output / "registered-install"
    demo_install(full_install)
    wad = full_install / "base/wad/DOOM.wad"
    wad.parent.mkdir(parents=True, exist_ok=True)
    user_wad = ROOT / "base/wad/DOOM.WAD"
    if user_wad.is_file():
        shutil.copy2(user_wad, wad)
    else:
        # The requested gate checks presence, not authentication. Use a tiny test
        # IWAD when the user's original has been removed to exercise demo mode.
        wad.write_bytes(struct.pack("<4sii", b"IWAD", 0, 12))
    full_log = launch(
        registered,
        full_install,
        [
            "wait 600",
            "spdevmap e2m1",
            "wait 600",
            "doom_demo",
            "savegame content_e2",
            "wait 10",
            "spdevmap e3m1",
            "wait 600",
            "savegame content_e3",
            "wait 10",
            "quit",
        ],
        "e1m1",
        demo=False,
    )
    if '"doom_demo" is:"0' not in full_log or any(
        f"Doom map ready: {name} " not in full_log for name in ("e2m1", "e3m1")
    ):
        raise RuntimeError("Installed WAD must enable both registered episodes")
    install = output / "demo-install"
    demo_install(install)
    if (install / "base/wad").exists():
        raise RuntimeError(f"Demo test installation must not contain a WAD directory: {install}")
    demo_home = output / "demo"
    saves = demo_home / "base" / "save"
    saves.mkdir(parents=True, exist_ok=True)
    for name in ("content_e2", "content_e3"):
        shutil.copy2(registered / "base" / "save" / f"{name}.svg", saves / f"{name}.svg")
    demo_log = launch(
        demo_home,
        install,
        [
            "wait 600",
            "doom_demo",
            "set doom_demo 0",
            "spmap e2m1",
            "map e3m1",
            "devmap E2M5",
            "spdevmap e3m9",
            "loadgame content_e2",
            "spmap save/content_e3.svg",
            "doom_demo",
            "savegame content_e1",
            "wait 10",
            "spmap e1m9",
            "wait 600",
            "loadgame content_e1",
            "wait 600",
            "doom_demo",
            "quit",
        ],
        "e1m1",
        demo=True,
    )
    if demo_log.count('"doom_demo" is:"1') != 3:
        raise RuntimeError("Demo detection must resist set commands and filesystem restarts")
    for name in ("e2m1", "e3m1", "E2M5", "e3m9"):
        if f"Can't load map {name}: episodes 2 and 3 require wad/DOOM.wad." not in demo_log:
            raise RuntimeError(f"Direct map gate missing: {name}")
    for name in ("content_e2", "content_e3"):
        if f"Can't load savegame save/{name}.svg: episodes 2 and 3 require" not in demo_log:
            raise RuntimeError(f"Save gate missing: {name}")
    if "Doom map ready: e2" in demo_log or "Doom map ready: e3" in demo_log:
        raise RuntimeError("Registered map loaded in demo mode")
    if demo_log.count("Doom map ready: e1m1 ") != 2 or "Doom map ready: e1m9 " not in demo_log:
        raise RuntimeError("Episode 1 maps and saves must remain usable in demo mode")
    report = {
        "registered_episodes": ["e2m1", "e3m1"],
        "demo_map_commands_blocked": 4,
        "registered_saves_blocked": 2,
        "demo_episode1_save_restored": True,
        "registered_detection_used_user_wad": user_wad.is_file(),
    }
    (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
