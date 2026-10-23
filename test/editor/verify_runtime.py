"""Verify the integrated workspace through the native engine's OpenGL renderer."""

import argparse
import json
from pathlib import Path
import subprocess
import shutil
import re
import struct
import wave
import ctypes
from ctypes import wintypes
import time

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]


def verify_window_controls(process, base):
    """Drive only this regression process's HWND through native system commands."""
    user = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = (callback_type, wintypes.LPARAM)
    user.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
    user.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
    user.GetClassNameW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
    user.GetWindowLongW.argtypes = (wintypes.HWND, ctypes.c_int)
    user.GetWindowRect.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.RECT))
    user.GetClientRect.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.RECT))
    user.SendMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)
    user.SendMessageW.restype = ctypes.c_ssize_t
    user.ShowWindow.argtypes = (wintypes.HWND, ctypes.c_int)
    user.IsZoomed.argtypes = (wintypes.HWND,)
    user.IsIconic.argtypes = (wintypes.HWND,)
    user.SetWindowPos.argtypes = (
        wintypes.HWND,
        wintypes.HWND,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_int,
        wintypes.UINT,
    )
    window = None
    sizes = {}
    handled = set()
    log_path = base / "doomconsole.log"

    def caption(handle):
        text = ctypes.create_unicode_buffer(32768)
        user.GetWindowTextW(handle, text, len(text))
        return text.value

    def rect(client=False):
        bounds = wintypes.RECT()
        query = user.GetClientRect
 if client else user.GetWindowRect
        if not query(window, ctypes.byref(bounds)):
            raise ctypes.WinError(ctypes.get_last_error())
        return (bounds.left, bounds.top, bounds.right, bounds.bottom)

    def command(value):
        user.SendMessageW(window, 0x0112, value, 0)  # WM_SYSCOMMAND

    @callback_type
    def find_window(handle, _):
        nonlocal window
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(handle, ctypes.byref(owner))
        class_name = ctypes.create_unicode_buffer(128)
        user.GetClassNameW(handle, class_name, len(class_name))
        if owner.value == process.pid and class_name.value == "Doom":
            window = handle
        return True

    deadline = time.monotonic() + 120
    while process.poll() is None:
        if time.monotonic() > deadline:
            raise RuntimeError("Native window control regression timed out")
        if window is None:
            user.EnumWindows(find_window, 0)
        log = log_path.read_text(errors="replace") if log_path.exists() else ""
        for stage in (
            "GAME_MAXIMIZE",
            "MAXIMIZE",
            "MINIMIZE",
            "RESTORE",
            "REMAXIMIZE",
            "GAME",
            "RESTART",
            "GAME_RESTART",
        ):
            marker = f"EDITOR_WINDOW_{stage}"
            if not re.search(rf"(?m)^{marker}\s*$", log) or stage in handled:
                continue
            if window is None:
                raise RuntimeError("Native game window was not found")
            handled.add(stage)
            if stage in ("RESTART", "GAME_RESTART"):
                user.EnumWindows(find_window, 0)
            if stage in ("GAME", "GAME_RESTART"):
                if caption(window) != "Doom" or not user.IsZoomed(window):
                    raise RuntimeError("Return to game did not restore native title/style")
                if rect(True)[2:] != sizes["editor_maximized"]:
                    raise RuntimeError(
"Return to game did not retain maximized native bounds")
                command(0xF020)
                if not user.IsIconic(window):
                    raise RuntimeError("Game minimize control failed")
                command(0xF120)
                if not user.IsZoomed(window) or user.IsIconic(window):
                    raise RuntimeError("Game restore control lost maximization")
                capture = "editor_game_after" if stage == "GAME" else "editor_game_restart"
                sizes[capture] = rect(True)[2:]
                continue
            if stage == "GAME_MAXIMIZE":
                if user.GetWindowLongW(window, -16) & 0x00070000 != 0x00070000:
                    raise RuntimeError("Game has no native min/max/resize controls")
                command(0xF030)
                if not user.IsZoomed(window):
                    raise RuntimeError("Game maximize control failed")
                sizes["editor_game_before"] = rect(True)[2:]
                continue
            expected = f"DoomEdit: {base / 'maps/e1m1.dwd'}"
            if caption(window) != expected:
                raise RuntimeError("Native control transition lost the editor caption")
            if stage == "RESTART":
                if not user.IsZoomed(window):
                    raise RuntimeError("Renderer restart lost the maximized window state")
                sizes["editor_restart"] = rect(True)[2:]
            elif stage == "MAXIMIZE":
                if not user.IsZoomed(window):
                    raise RuntimeError("Opening the editor lost the maximized game state")
                if user.GetWindowLongW(window, -16) & 0x00070000 != 0x00070000:
                    raise RuntimeError("Editor has no native min/max/resize controls")
                command(0xF030)  # SC_MAXIMIZE
                if not user.IsZoomed(window):
                    raise RuntimeError("Maximize control did not maximize the actual engine")
                bounds = rect(True)
                s
izes["editor_maximized"] = bounds[2:]
            elif stage == "MINIMIZE":
                command(0xF020)  # SC_MINIMIZE
                if not user.IsIconic(window):
                    raise RuntimeError("Minimize control did not minimize the actual engine")
            elif stage == "RESTORE":
                command(0xF120)  # SC_RESTORE
                if user.IsIconic(window) or not user.IsZoomed(window):
                    raise RuntimeError("Restore did not retain the maximized editor state")
                command(0xF120)
                if user.IsZoomed(window):
                    raise RuntimeError("Restore did not unmaximize the actual engine")
                if not user.SetWindowPos(window, None, 20, 20, 1100, 680, 0x14):
                    raise ctypes.WinError(ctypes.get_last_error())
                bounds = rect(True)
                sizes["editor_resized"] = bounds[2:]
            else:
                command(0xF030)
                if not user.IsZoomed(window):
                    raise RuntimeError("Editor maximize after resize failed")
        time.sleep(0.02)
    if process.returncode or len(handled) != 8:
        raise RuntimeError(f"Window regression incomplete: exit={process.returncode}, {handled}")
    return sizes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", default="release")
    parser.add_argument("--window-controls", action="store_true")
    parser.add_argument("--runtime-directory", type=Path, default=ROOT)
    args = parser.parse_args()
    output = ROOT / "out" / f"editor-runtime-{args.label}"
    base = output / "base"
    base.mkdir(parents=True, exist_ok=True)
    # An isolated executable root exercises the real console commands without
    # publishing fixture outputs or profiles into the user's campaign tree.
    for name in ("Doom.exe", "toolsx86.dll"):
        shutil.copy2(args.runtime_directory / name, output / name)
    for name in ("cgamex86.dll", "qagam
ex86.dll", "uix86.dll"):
        shutil.copy2(args.runtime_directory / "base" / name, base / name)
        # The legacy native DLL loader searches beside the executable before
        # fs_basepath/base, and does not consult fs_homepath. Keep this run's
        # modules isolated even while another configuration is being built.
        shutil.copy2(args.runtime_directory / "base" / name, output / name)
    for directory in ("maps", "wad", "aas"):
        (base / directory).mkdir(exist_ok=True)
    shutil.copy2(ROOT / "base/maps/e1m1.dwd", base / "maps/e1m1.dwd")
    shutil.copy2(ROOT / "base/maps/e1m2.dwd", base / "maps/e1m2.dwd")
    shutil.copy2(ROOT / "base/wad/DOOM.WAD", base / "wad/DOOM.WAD")
    fixture = ROOT / "out/editor-tests" / f"{args.label}-integrated/base/maps/worktest.dwd"
    shutil.copy2(fixture, base / "maps/worktest.dwd")
    for profile in (ROOT / "base/aas").glob("*.cfg"):
        shutil.copy2(profile, base / "aas" / profile.name)
    (base / "sound").mkdir(exist_ok=True)
    with wave.open(str(base / "sound/editorhold.wav"), "wb") as audio:
        audio.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
        audio.writeframes(struct.pack("<h", 1000) * (16000 * 30))
    commands = [
        "set com_maxfps 60",
        "set con_notifytime 0",
        "wait 180",
        "editor e1m2",
        "wait 20",
        "screenshot editor_e1m2",
        "wait 2",
        "editor",
        "wait 4",
        "spdevmap e1m1",
        "wait 120",
        "god",
        "set doom_ai_disable 1",
        "wait 10",
        "screenshot editor_game_before",
        "wait 2",
        "stopsound",
        "music music/doom/d_e1m1.mid",
        "play sound/editorhold.wav",
        "wait 10",
        "editor e1m1",
        "wait 5",
        "echo EDITOR_PAUSE_BEGIN",
        "s_info",
        "s_musicinfo",
        "cl_paused",
        "sv_paused",
        "savegame editor_pause_a",
        "wait 120",
        "s_info",
        "s_musicinfo",
        "savegame
 editor_pause_b",
        "echo EDITOR_PAUSE_END",
        "editor_active",
        "screenshot editor_workspace",
        "wait 2",
        "editor",
        "wait 20",
        "editor_active",
        "screenshot editor_game_after",
        "echo EDITOR_RESUME_BEGIN",
        "s_info",
        "s_musicinfo",
        "cl_paused",
        "sv_paused",
        "savegame editor_resumed",
        "echo EDITOR_RESUME_END",
        "wait 2",
        "doommenu",
        "wait 4",
        "editor",
        "wait 16",
        "set r_customwidth 1440",
        "set r_customheight 900",
        "vid_restart",
        "wait 80",
        "screenshot editor_restart",
        "wait 2",
        "editor",
        "wait 20",
        "editor_active",
        "screenshot editor_game_restart",
        "echo EDITOR_PRIOR_PAUSE_BEGIN",
        "cl_paused",
        "s_info",
        "echo EDITOR_PRIOR_PAUSE_END",
        "wait 2",
        "dmap worktest",
        "wait 480",
        "aasbuild worktest",
        "wait 480",
        "editor ../escape",
        "dmap ../escape",
        "aasbuild ../escape",
        "wait 4",
        "quit",
    ]
    if args.window_controls:
        index = commands.index("screenshot editor_game_before")
        commands[index:index] = ["echo EDITOR_WINDOW_GAME_MAXIMIZE", "wait 45"]
        index = commands.index("screenshot editor_workspace") + 2
        commands[index:index] = [
            "echo EDITOR_WINDOW_MAXIMIZE",
            "wait 90",
            "screenshot editor_maximized",
            "wait 2",
            "echo EDITOR_WINDOW_MINIMIZE",
            "wait 90",
            "echo EDITOR_WINDOW_RESTORE",
            "wait 90",
            "screenshot editor_resized",
            "wait 2",
            "echo EDITOR_WINDOW_REMAXIMIZE",
            "wait 45",
        ]
        index = commands.index("screenshot editor_game_after")
        commands[index:index] = ["echo EDITOR_WINDOW_GAME", "wait 4"]
        index = commands.index("screenshot editor_r
estart")
        commands[index:index] = ["echo EDITOR_WINDOW_RESTART", "wait 4"]
        index = commands.index("screenshot editor_game_restart")
        commands[index:index] = ["echo EDITOR_WINDOW_GAME_RESTART", "wait 4"]
    (base / "verify.cfg").write_text("\n".join(commands) + "\n", encoding="ascii")
    command = [str(output / "Doom.exe")]
    for key, value in {
        "fs_basepath": ROOT,
        "fs_homepath": output,
        "vm_game": 0,
        "vm_cgame": 0,
        "vm_ui": 0,
        "com_blindlyLoadDLLs": 1,
        "sv_pure": 0,
        "logfile": 2,
        "r_fullscreen": 0,
        "r_hiddenWindow": 0 if args.window_controls else 1,
        "r_mode": -1,
        "r_customwidth": 1440,
        "r_customheight": 900,
        "r_smp": 0,
        "r_gamma": 1,
        "r_intensity": 1,
        "r_overBrightBits": 0,
        "in_mouse": 0,
        "in_joystick": 0,
        "net_port": 27982,
        "com_hunkMegs": 256,
    }.items():
        command.extend(("+set", key, str(value)))
    command.extend(("+exec", "verify.cfg"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    (base / "doomconsole.log").unlink(missing_ok=True)
    process = subprocess.Popen(command, cwd=output, startupinfo=startup)
    window_sizes = {}
    try:
        if args.window_controls:
            window_sizes = verify_window_controls(process, base)
        elif process.wait(timeout=120):
            raise RuntimeError(f"Native editor test failed; see {base}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    log = (base / "doomconsole.log").read_text(errors="replace")
    required = (
        "Integrated DWD editor initialized (API 2; engine renderer)",
        "Editor shown; Return to game / F12 preserves the document",
        "Returned to game; editor document retained",
        '"editor_active" is:"1',
        '"editor_active"
 is:"0',
        "Map names must contain 1..32",
        "Built BSP and all four AAS profiles:",
    )
    for text in required:
        if text not in log:
            raise RuntimeError(f"Missing runtime evidence: {text}; see {base}")
    for text in ("Engine renderer could not", "Editor: ", "GL_INVALID", "GL_OUT_OF_MEMORY"):
        if text in log:
            raise RuntimeError(f"Editor runtime error: {text}; see {base}")
    if log.count("Built BSP and all four AAS profiles:") != 2:
        raise RuntimeError("Console dmap/aasbuild did not both finish")
    if not (base / "maps/worktest.bsp").is_file() or any(
        not (base / "maps" / f"worktest_b{world}.aas").is_file() for world in range(4)
    ):
        raise RuntimeError("Integrated console compilers did not produce all worlds")

    def section(name):
        return log.split(f"EDITOR_{name}_BEGIN", 1)[1].split(f"EDITOR_{name}_END", 1)[0]

    paused, resumed = section("PAUSE"), section("RESUME")
    audio_pattern = r"Doom audio state: editorPaused=(\d+) voices=(\d+) sampleOffset=(\d+)"
    paused_audio = re.findall(audio_pattern, paused)
    resumed_audio = re.findall(audio_pattern, resumed)
    if len(paused_audio) != 2 or paused_audio[0] != paused_audio[1]:
        raise RuntimeError("Sound playback position advanced while editing")
    if paused_audio[0][0] != "1" or int(paused_audio[0][1]) < 1:
        raise RuntimeError("Editor did not pause an active sound")
    if (
        not resumed_audio
        or resumed_audio[0][0] != "0"
        or int(resumed_audio[0][2]) <= int(paused_audio[0][2])
    ):
        raise RuntimeError("Sound did not resume from its retained position")
    paused_music = re.findall(r"Doom music: .*position=(\d+) loops=(\d+)", paused)
    resumed_music = re.findall(r"Doom music: .*position=(\d+) loops=(\d+)", resumed)
    if len(paused_music) != 2 or paused_music[0] != paused_music[1]:
        raise RuntimeError("Music advanced while editing")
    if not resumed_music or in
t(resumed_music[0][0]) <= int(paused_music[0][0]):
        raise RuntimeError("Music did not resume from its retained position")
    times = [
        struct.unpack_from("<i", (base / "save" / f"{name}.svg").read_bytes(), 68)[0]
        for name in ("editor_pause_a", "editor_pause_b", "editor_resumed")
    ]
    if times[0] != times[1] or times[2] <= times[1]:
        raise RuntimeError(f"Game simulation did not pause/resume: {times}")
    if '"cl_paused" is:"1' not in section("PRIOR_PAUSE"):
        raise RuntimeError("Returning from editor lost the existing pause state")
    if "editorPaused=0" not in section("PRIOR_PAUSE"):
        raise RuntimeError("Audio remained suspended after closing the editor")
    captures = {}
    capture_names = [
        "editor_e1m2",
        "editor_game_before",
        "editor_workspace",
        "editor_game_after",
        "editor_restart",
        "editor_game_restart",
        *window_sizes,
    ]
    for name in dict.fromkeys(capture_names):
        with Image.open(base / "screenshots" / f"{name}.tga") as image:
            captures[name] = image.convert("RGB")
        captures[name].save(output / f"{name}.png")
        if name in window_sizes and captures[name].size != window_sizes[name]:
            raise RuntimeError(f"Renderer did not follow the native client size for {name}")
    workspace = captures["editor_workspace"]
    if (
        workspace.width < 1000
        or len(workspace.getcolors(workspace.width * workspace.height) or []) < 500
    ):
        raise RuntimeError("Editor workspace is blank or has wrong dimensions")
    for name in ("editor_game_before", "editor_game_after", "editor_game_restart"):
        if (
            workspace.size == captures[name].size
            and not ImageChops.difference(workspace, captures[name]).getbbox()
        ):
            raise RuntimeError(f"Editor/game switch did not change output: {name}")
    # The right view must contain actual textured scene pixels, not only UI chro
me.
    preview = workspace.crop(
        (
            int(workspace.width * 0.3),
            int(workspace.height * 0.3),
            int(workspace.width * 0.73),
            int(workspace.height * 0.82),
        )
    )
    if len(preview.getcolors(preview.width * preview.height) or []) < 400:
        raise RuntimeError("3D preview lacks textured geometry")
    report = {
        "renderer": "engine OpenGL",
        "checks": [
            "DWD open and tabbed workspace",
            "textured 3D preview",
            "editor/game/editor state transitions",
            "renderer restart and GPU resource recreation",
            "map-name validation",
            "game save times frozen during editing and advancing after return",
            "sound sample cursor and music decoder position paused/resumed",
            "pre-existing pause restored after renderer restart",
            "in-process dmap and aasbuild console commands",
            "editor/menu/map/editor renderer resource lifecycle",
        ],
        "captures": list(captures),
        "native_window_client_sizes": window_sizes,
    }
    (output / "verification.json").write_text(json.dumps(report, indent=2))
    print(f"Integrated editor OpenGL runtime passed: {output}")


if __name__ == "__main__":
    main()
