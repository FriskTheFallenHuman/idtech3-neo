# Progress

What is being worked on right now, what is in progress and what is planned.
Plain English only. Update this file as work moves forward.

## Working on right now

- Task 2 (Q3VM removal): done. The engine and the game modules talk through
  plain C import and export tables (GetGameAPI, GetCGameAPI, GetUIAPI), CI
  compiles everything and the only build error left is the sound swap below.
- FluidSynth swap: TiMidity is out and FluidSynth 2.6.1 replaces it. The
  engine loads libfluidsynth-3.dll at runtime through GetProcAddress, so the
  DLL is optional and the engine still boots without it (OPL2 and PCM music
  keep working). The SoundFont is base/sound/timidity/TimGM6mb.sf2 and the
  MIDI backend cvar value is "fluidsynth" now. Waiting on CI to prove the
  build green.
- bin/base layout: the game modules and the aas profiles are copied into
  bin/base only, no longer into the repository base/. Sys_LoadDll searches
  the exe folder's base directory, so bin/Doom.exe finds the modules next to
  itself.

## In progress

- Task 1 (SCons build + GitHub auto builds):
  - SConstruct at the root, helper code in site_scons/, source lists moved
    over from the old CMake files with the folder paths fixed.
  - Binaries land in bin/ at the root: Doom.exe, cgamex86.dll, qagamex86.dll,
    uix86.dll, toolsx86.dll, and the tests in bin/tests/.
  - The game modules are copied into bin/base, where the engine loads them.
  - OpenAL Soft: the bundled package is prebuilt, the engine links
    libs/Win32/OpenAL32.lib and the build ships the bundled DLL as
    OpenAL32.dll. No CMake anywhere in the build.
  - FluidSynth: headers are bundled, the DLL is loaded at runtime, no import
    library is linked. Non-Windows systems fetch the pinned sources.
  - clang-cl is the preferred compiler, plain MSVC is the fallback. Windows
    x86 only for now (the game module ABI is still 32-bit, see task 4).
  - "scons tests" builds the tests, "scons test" runs them.
  - What is missing: a green build after the FluidSynth swap.

- Task 4 (warnings): the Windows x86 build compiles warning-free with
  clang-cl. The 64-bit and Linux builds are still open.

## Planned next, in priority order

1. Get the CI build green after the FluidSynth swap.
2. Auto download and replacement of dependencies (tasks 9 and 10).
3. Botlib as a DLL with the parser in the engine (task 3).
4. Full C++ with idLib (tasks 5 and 8).

## Not started yet

- Task 3 (botlib as a DLL, parser into the engine)
- Task 6 (extractfuncs as a pre-step for the game DLL)
- Task 7 (Doom 3 audio system)
- Task 11 (QuakeC driving the game modules)

## Done

- Task 2: Q3VM removed, plain C import/export between the engine and the
  modules (vm.cpp, vm_interpreted.cpp, vm_x86.cpp, vm_local.h, vm_args.h,
  the syscall tables and bg_lib.cpp deleted; GetGameAPI / GetCGameAPI /
  GetUIAPI entry points; restart = unload and reload the DLL).
- Task 1: SCons build replaces CMake, CI builds Windows x86 and saves
  build.log, the digest workflow turns it into build-digest.md.
- GitHub workflow that builds the project and saves build.log.
- GitHub workflow that turns a build.log from the repo root into a short report.
- VSCode setup for building, running, debugging and testing the engine.
- Tasks 9 and 10 so far: jpeg-6, libpng, zlib, ft2 and timidy are out of
  third-party. stb_image, stb_image_write, stb_truetype and miniz do their
  jobs, OpenAL Soft and FluidSynth fetch their pinned sources on
  non-Windows systems.
- All warnings fixed in the Windows x86 clang-cl build.
- Project planning docs (TODO.md, PROGRESS.md, NOTES.md, CLAUDE.md, COPILOT.md).
