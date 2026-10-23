"""Verify the Win32 DLL entry point with Unicode response and result paths."""

import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, default=ROOT / "toolsx86.dll")
    parser.add_argument("--label", default="release")
    args = parser.parse_args()
    module = args.module.resolve(strict=True)
    folder = ROOT / "out/compiler-entry" / args.label / "unicode \u6d4b\u8bd5"
    folder.mkdir(parents=True, exist_ok=True)
    host = Path(os.environ["SystemRoot"]) / "SysWOW64/rundll32.exe"
    log = folder / "compiler \u65e5\u5fd7.log"
    for valid in (True, False):
        response = folder / f"request \u6587\u4ef6 {valid}.txt"
        result_path = response.with_suffix(".txt.exit")
        error_path = response.with_suffix(".txt.error")
        for output in (result_path, error_path):
            output.unlink(missing_ok=True)
        lines = [str(ROOT), str(log), "dmap", "--help"] if valid else ["incomplete"]
        response.write_text("\n".join(lines) + "\n", encoding="utf-8")
        subprocess.run(
            [str(host), f"{module},DoomTools_Run", str(response)],
            check=True,
            timeout=30,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        expected = "0" if valid else "1"
        if result_path.read_text() != expected:
            raise RuntimeError(f"Unexpected DLL compiler result: {response}")
        if valid:
            if error_path.exists() or "dmap [--base" not in log.read_text():
                raise RuntimeError("Valid Unicode response did not run the compiler")
        elif "Invalid compiler response file" not in error_path.read_text():
            raise RuntimeError("Malformed Unicode response did not report an error")
    print("DLL entry passed Unicode/space paths, compiler invocation and malformed response checks")


if __name__ == "__main__":
    main()
