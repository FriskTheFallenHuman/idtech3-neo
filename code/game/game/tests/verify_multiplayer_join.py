"""Connect a second native Doom client over UDP to a listen server with team bots."""

import argparse
import json
from pathlib import Path
import subprocess
import time

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]


def launch(binaries, home, name, port, commands, start):
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
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
        "developer": 1,
        "name": name,
        "net_port": port,
        "com_maxfps": 125,
        "r_fullscreen": 0,
        "r_hiddenWindow": 1,
        "r_mode": -1,
        "r_customwidth": 1280,
        "r_customheight": 720,
        "r_smp": 0,
        "in_mouse": 0,
        "g_gametype": 3,
        "sv_maxclients": 16,
        "doom_botCount": 2,
        "doom_botSkill": 3,
        "bot_enable": 1,
        "g_doWarmup": 0,
        "fraglimit": 0,
        "timelimit": 0,
        "g_password": "",
    }.items():
        command += ["+set", key, str(value)]
    command += start + ["+exec", "verify.cfg"]
    return subprocess.Popen(
        command,
        cwd=binaries,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binaries", type=Path, default=ROOT)
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    output = ROOT / "out" / f"multiplayer-join-{args.label}"
    binaries = args.binaries.resolve()
    processes = []
    try:
        host = launch(
            binaries,
            output / "host",
            "DoomHost",
            27977,
            [
                "wait 180",
                "god",
                "notarget",
                "botstatus",
                "wait 1200",
                "status",
                "wait 1800",
                "botstatus",
                "doommenu",
                "wait 20",
                "screenshot menu_order",
                "doommenu multiplayer",
                "wait 20",
                "screenshot multiplayer_settings",
                "wait 20",
                "quit",
            ],
            ["+devmap", "e1m1"],
        )
        processes.append(host)
        host_log = output / "host/base/doomconsole.log"
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if host.poll() is not None:
                raise RuntimeError(f"Host exited before client connection: {host_log}")
            if host_log.exists() and "Going from CS_PRIMED to CS_ACTIVE" in host_log.read_text(
                errors="replace"
            ):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError(f"Host never became active: {host_log}")
        guest = launch(
            binaries,
            output / "guest",
            "DoomGuest",
            27978,
            [
                "wait 350",
                "god",
                "notarget",
                "screenshot joined_server",
                "wait 200",
                "+forward",
                "wait 120",
                "-forward",
                "wait 20",
                "+attack",
                "wait 160",
                "-attack",
                "wait 100",
                "screenshot joined_combat",
                "wait 100",
                "quit",
            ],
            ["+connect", "127.0.0.1:27977"],
        )
        processes.append(guest)
        for process in processes:
            if process.wait(timeout=90):
                raise RuntimeError(f"Native process exited unsuccessfully: {output}")
        host_text = host_log.read_text(errors="replace")
        guest_text = (output / "guest/base/doomconsole.log").read_text(errors="replace")
        if "Going from CS_PRIMED to CS_ACTIVE for DoomGuest" not in host_text:
            raise RuntimeError("Guest did not finish the UDP gamestate/snapshot handshake")
        if "Doom FX: 72/72" not in guest_text or "ERROR:" in guest_text:
            raise RuntimeError("Guest failed to initialize the native Doom client")
        if "Doom bot roster: 2 mode=3" not in host_text:
            raise RuntimeError("Team bots did not survive a remote human joining")
        captures = []
        for role in ("host", "guest"):
            for source in (output / role / "base/screenshots").glob("*.tga"):
                target = output / role / f"{source.stem}.png"
                with Image.open(source) as picture:
                    picture.save(target)
                captures.append(str(target.relative_to(output)))
        if len(captures) < 4:
            raise RuntimeError("Menu and remote-client captures missing")
        (output / "validation.json").write_text(
            json.dumps(
                {
                    "transport": "UDP 127.0.0.1:27977",
                    "players": ["DoomHost", "DoomGuest"],
                    "bots": 2,
                    "mode": "team deathmatch",
                    "captures": captures,
                    "passed": True,
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        print(f"Native multiplayer host/join passed: {output}")
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)


if __name__ == "__main__":
    main()
