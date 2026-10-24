"""Native single-player exits, intermission input, inventory carry and repeated map loads."""

import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    home = ROOT / f"out/level-flow-{args.label}"
    base = home / "base"
    base.mkdir(parents=True, exist_ok=True)
    # The exit inserts spmap ahead of the pending cfg; verify that later
    # commands resume inside the destination rather than blocking its load.
    (base / "start.cfg").write_text(
        "\n".join(
            (
                "set com_maxfps 125",
                "set developer 1",
                "set con_notifytime 0",
                "wait 250",
                "god",
                "give all",
                "set doom_ai_disable 1",
                "setviewpos 2952 -4768 0.125 180",
                "wait 80",
                "+activate",
                "wait 30",
                "-activate",
                "wait 80",
                "screenshot level_counting",
                "+attack",
                "wait 20",
                "-attack",
                "wait 80",
                "screenshot level_statistics",
                "+attack",
                "wait 20",
                "-attack",
                "wait 80",
                "screenshot level_entering",
                "wait 1200",
                "exec finish.cfg",
                "",
            )
        )
    )
    (base / "finish.cfg").write_text(
        "\n".join(
            (
                "wait 200",
                "screenshot level_e1m2",
                "s_info",
                "s_musicinfo",
                "spdevmap e1m3",
                "wait 250",
                "spmap e1m2",
                "wait 250",
                "screenshot level_e1m2_again",
                "quit",
                "",
            )
        )
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
        ("r_mode", 1),
        ("r_hiddenWindow", 1),
        ("in_mouse", 0),
        ("in_joystick", 0),
        ("net_port", 27976),
    ):
        command.extend(("+set", name, str(value)))
    command.extend(("+spdevmap", "e1m1", "+exec", "start.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(x) for x in command], cwd=ROOT, startupinfo=startup)
    (home / "process-id.txt").write_text(str(process.pid))
    log = base / "doomconsole.log"
    try:
        result = process.wait(timeout=60)
        if result:
            raise RuntimeError(f"Native level test failed with {result}; see {log}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    text = log.read_text(errors="replace")
    phases = [
        int(record.split()[-2]) for record in re.findall(r"Doom intermission: ([\d ]+)", text)
    ]
    if phases != [0, 1, 2, 3]:
        raise RuntimeError(f"Intermission phase sequence differs: {phases}")
    if len(re.findall(r"Doom map ready: e1m2 ", text)) != 2 or "Doom map ready: e1m3 " not in text:
        raise RuntimeError("Repeated client/server map reload did not complete")
    if not re.search(
        r"Doom inventory carried into e1m2: health=100 armor=200 weapons=255 bullets=200", text
    ):
        raise RuntimeError("Inventory carry differs from completed level")
    if "unsupported or malformed" in text or "OpenAL: cannot" in text:
        raise RuntimeError("Native sound decoding or upload failed")
    if (
        "Doom OPL2 music: music/doom/d_inter.mid" not in text
        or "Doom OPL2 music: music/doom/d_e1m2.mid" not in text
    ):
        raise RuntimeError("Intermission and destination music did not start")
    (home / "verification.json").write_text(
        json.dumps(
            {"passed": True, "phases": phases, "destination": "e1m2", "map_loads": 4}, indent=2
        )
    )
    print(
        f"Native level exit, counters, entering screen, carry, music and repeated E1M2 loads passed: {home}"
    )


if __name__ == "__main__":
    main()
