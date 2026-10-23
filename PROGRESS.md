# Progress

What is being worked on right now, what is in progress and what is planned.
Plain English only. Update this file as work moves forward.

## Working on right now

- Task 2 (Q3VM removal): done. The engine and the game modules talk through
  plain C import and export tables (GetGameAPI, GetCGameAPI, GetUIAPI) and
  CI compiles everything warning-free.
- FluidSynth swap: TiMidity is out and FluidSynth 2.6.1 replaces it. The
  engine loads libfluidsynth-3.dll at runtime through GetProcAddress, so the
  DLL is optional and the engine still boots without it (OPL2 and PCM music
  keep working). The prebuilt Win32 DLLs (libfluidsynth-3.dll with its
  sndfile.dll and SDL3.dll dependencies) are in the repository now and scons
  ships the three of them next to the engine and the tests, so the
  "fluidsynth" music backend works out of the box. The SoundFont is
  base/sound/timidity/TimGM6mb.sf2 and the MIDI backend cvar value is
  "fluidsynth" now. The CI build is green with no warnings.
- bin/base layout: the game modules and the aas profiles are copied into
  bin/base, no longer into the repository base/.

## Planned next, in priority order

1. Auto download and replacement of dependencies (tasks 9 and 10).
2. Botlib as a DLL with the parser in the engine (task 3).
3. Full C++ with idLib (tasks 5 and 8).
4. The x64 and Linux builds (task 12).

## Not started yet

- Task 3 (botlib as a DLL, parser into the engine)
- Task 6 (extractfuncs as a pre-step for the game DLL)
- Task 7 (Doom 3 audio system)
- Task 11 (QuakeC driving the game modules)
- Task 12 (x64 and Linux builds)

## Done

- Task 1: SCons build replaces CMake, CI builds Windows x86 and saves
  build.log, the digest workflow turns it into build-digest.md.
- Task 4: the Windows x86 build is warning-free with clang-cl and CI keeps
  it green. The x64 and Linux builds and the warnings-as-errors switch are
  task 12 now.
- Task 2: Q3VM removed, plain C import/export between the engine and the
  modules (vm.cpp, vm_interpreted.cpp, vm_x86.cpp, vm_local.h, vm_args.h,
  the syscall tables and bg_lib.cpp deleted; GetGameAPI / GetCGameAPI /
  GetUIAPI entry points; restart = unload and reload the DLL).
- GitHub workflow that builds the project and saves build.log.
- GitHub workflow that turns a build.log from the repo root into a short report.
- VSCode setup for building, running, debugging and testing the engine.
- Tasks 9 and 10 so far: jpeg-6, libpng, zlib, ft2 and timidy are out of
  third-party. stb_image, stb_image_write, stb_truetype and miniz do their
  jobs, OpenAL Soft and FluidSynth fetch their pinned sources on
  non-Windows systems.
- All warnings fixed in the Windows x86 clang-cl build.
- Project planning docs (TODO.md, PROGRESS.md, NOTES.md, CLAUDE.md, COPILOT.md).
