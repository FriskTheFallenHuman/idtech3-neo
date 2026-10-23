"""Load E1M1 from a packaged installation after the native title menu starts."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", type=Path, default=ROOT / "build")
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    install = args.install.resolve()
    output = ROOT / "out" / f"pk3-runtime-{args.label}"
    base = output / "base"
    base.mkdir(parents=True, exist_ok=True)
    archive = install / "base/pak0.pk3"
    with zipfile.ZipFile(archive) as package:
        bad_entry = package.testzip()
        if bad_entry:
            raise RuntimeError(f"Corrupt PK3 entry: {bad_entry}")
        required = ["maps/e1m1.bsp"] + [f"maps/e1m1_b{i}.aas" for i in range(4)]
        for name in required:
            if not package.getinfo(name).file_size:
                raise RuntimeError(f"Empty packaged map asset: {name}")
            if (install / "base" / name).exists():
                raise RuntimeError(f"Loose map asset would mask the PK3 check: {name}")

    # Preserve the packaged preferences while keeping test writes and saves in out/.
    config = install / "base/doomconfig.cfg"
    if config.is_file():
        shutil.copy2(config, base / config.name)
    (base / "verify.cfg").write_text(
        "wait 40\nspmap e1m1\nwait 160\nscreenshot e1m1_pk3\n"
        "spmap __missing_pk3_test__\nwait 10\nquit\n",
        encoding="ascii",
    )
    command = [str(install / "Doom.exe")]
    settings = {
        "fs_homepath": output,
        "logfile": 2,
        "fs_debug": 1,
        "developer": 1,
        "r_fullscreen": 0,
        "r_hiddenWindow": 1,
        "in_mouse": 0,
        "in_joystick": 0,
        "com_maxfps": 125,
        "net_port": 27989,
    }
    for name, value in settings.items():
        command.extend(("+set", name, str(value)))
    command.extend(("+exec", "verify.cfg"))
    process = subprocess.Popen(
        command,
        cwd=install,
        creationflags=subprocess.CREATE_NO_WINDOW,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.STDOUT,
    )
    try:
        if process.wait(timeout=45):
            raise RuntimeError(f"Packaged runtime failed: {output}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    log = (base / "doomconsole.log").read_text(errors="replace")
    for marker in (
        "Doom map ready: e1m1 (85 sectors, 475 lines)",
        "entered the game",
        "Can't find map maps/__missing_pk3_test__.bsp",
        "Doom FX: 72/72",
    ):
        if marker not in log:
            raise RuntimeError(f"Packaged runtime check missing {marker!r}: {output}")
    for name in required:
        if f"FS_FOpenFileRead: {name} (found in '" not in log:
            raise RuntimeError(f"Map asset was not read from the PK3: {name}")
    if "ERROR:" in log or "^1ERROR" in log:
        raise RuntimeError(f"Packaged runtime reported an engine error: {output}")
    report = {
        "install": str(install),
        "pk3_integrity": True,
        "map": "e1m1",
        "sectors": 85,
        "lines": 475,
        "navigation_profiles": 4,
        "active_client": True,
        "missing_map_rejected": True,
    }
    (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
