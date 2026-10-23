"""Run the compilers embedded in toolsx86.dll from offline development scripts.

The editor uses a worker inside Doom.exe. Offline scripts use Windows' existing
32-bit DLL host so 64-bit Python can access the same native implementation.
"""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def compiler_command(tool, module=None):
    """Return an argv prefix suitable for existing checked subprocess helpers."""
    return [
        sys.executable,
        str(Path(__file__).resolve()),
        "--module",
        str(module or ROOT / "toolsx86.dll"),
        tool,
    ]


def run_compiler(module, tool, arguments, log=None, timeout=600):
    module = Path(module).resolve(strict=True)
    if tool not in ("dmap", "aasbuild"):
        raise ValueError(f"Unknown compiler: {tool}")
    scratch = ROOT / "out/compiler-scripts"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{tool}-", dir=scratch) as temporary:
        folder = Path(temporary)
        log_path = Path(log).resolve() if log else folder / "compile.log"
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text("", encoding="utf-8")
        lines = [str(module.parent), str(log_path), tool, *(str(arg) for arg in arguments)]
        if any("\n" in line or "\r" in line or "\0" in line for line in lines):
            raise ValueError("Compiler arguments cannot contain newlines or NUL")
        response = folder / "request.txt"
        response.write_text("\n".join(lines) + "\n", encoding="utf-8")
        windows = Path(os.environ["SystemRoot"])
        host = windows / "SysWOW64/rundll32.exe"
        if not host.is_file():
            host = windows / "System32/rundll32.exe"
        subprocess.run(
            [str(host), f"{module},DoomTools_Run", str(response)],
            check=True,
            timeout=timeout,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        error_path = response.with_suffix(".txt.error")
        if error_path.is_file():
            raise RuntimeError(error_path.read_text())
        result_path = response.with_suffix(".txt.exit")
        if not result_path.is_file():
            raise RuntimeError("Native compiler did not return a result")
        result = int(result_path.read_text())
        output = log_path.read_text(encoding="utf-8", errors="replace")
        return subprocess.CompletedProcess([tool, *arguments], result, output, "")


def main():
    parser = argparse.ArgumentParser(description=__doc__, add_help=False)
    parser.add_argument("--module", type=Path, default=ROOT / "toolsx86.dll")
    parser.add_argument("tool", choices=("dmap", "aasbuild"))
    args, arguments = parser.parse_known_args()
    result = run_compiler(args.module, args.tool, arguments)
    print(result.stdout, end="")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
