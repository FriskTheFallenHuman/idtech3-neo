# TODO

The plan for this source port. The big goal: get Quake 3 and Return to Castle
Wolfenstein running on the id Tech 3 branch that Enemy Territory uses.

Rules for every task: keep it simple, keep the build working, make sure the
engine still boots after each change. Update PROGRESS.md as work moves along.

Note on folder names: the plan below says `src/` for the engine folder. In
the repository today that folder is called `code/`. They are the same thing until
the folder gets renamed.

## 1. Replace CMake with SCons

- Delete the CMake build (`code/CMakeLists.txt`, `code/CMakePresets.json`, `code/cmake/`).
- Write a new SCons build. One `SConstruct` at the root, helper code in `site_scons/`.
- Put every finished binary in `bin/` at the root of the repository (engine exe, game dlls, tools).
- Build on both Windows and Linux, in both x86 and x64.
- Priority 1 together with the GitHub auto builds.
- Done. The SCons build replaces CMake and CI runs it on Windows x86, saving
  build.log every run. The Linux and x64 parts live in task 12.

## 2. Remove Q3VM, use plain C import/export

- Delete the old bytecode VM from the engine (`code/qcommon/vm.cpp`, `vm_interpreted.cpp`, `vm_x86.cpp`, `vm_local.h`, `vm_args.h`).
- Delete the VM side of the game code too (vmMain entry points, syscall tables, `bg_lib.cpp`, `cg_syscalls.cpp`, `g_syscalls.cpp` and `ui_syscalls.cpp`).
- The game, cgame and ui become normal native libraries that import engine functions and export their own, no bytecode in between, no weird enum base export/import tables.
- This also removes the reason the engine is locked to 32-bit today.
- Done. Each module exports one entry point (GetGameAPI, GetCGameAPI,
  GetUIAPI) and the engine passes it a plain C import struct. The modules
  load from bin/base, and a module restart is an unload plus a fresh load.

## 3. Botlib becomes a DLL, the parser moves into the engine

- Today botlib is a static library inside the engine (`code/game/botlib`).
- Split it: the parser side (`l_script`, `l_precomp`, `l_struct`) moves into the engine, because the engine needs to parse things itself.
- The rest of botlib becomes its own DLL that the engine loads at startup, like any other module.
- Botlib must keep working without the bot AI inside the engine.

## 4. Build and run on both x64 and x86, fix all warnings

- Get the engine to compile and run in 64-bit and 32-bit.
- Fix every warning the compilers print on the way, do not just hide them.
- Turn warnings into errors in the build once the warning count is zero.
- The 32-bit lock exists because of the old VM, task 2 removes it.
- Done. The Windows x86 build compiles warning-free with clang-cl and CI
  keeps it green. The x64 and Linux builds and the warnings-as-errors switch
  moved to task 12.

## 5. Port Doom 3's idLib

- idLib is already vendored in `code/idLib` (the one from the BFG edition).
- id Software did this on their RTCW branch but ran out of time and put it back. Finish what they started.
- Hook idLib into the engine first (strings, containers, math, files), then let engine code use it step by step.
- idLib is GPLv3, same license family as the engine, so it is fine to use here.

## 6. Make extractfuncs work as a pre-step for the game DLL

- extractfuncs does not exist in this repo yet, it has to be brought in.
- It scans the game source and writes a generated file that the game DLL needs for save games.
- It must run every time before the game DLL is built. If its output changes, the game DLL rebuilds.

## 7. Port the Doom 3 audio system

- Port the full Doom 3 sound system into the engine.
- Use `references/doom3.gpl` as the source, that version has the better OpenAL support.
- Replace the current sound code in `code/sound` with the Doom 3 system once the new one works.

## 8. Turn the C style code into proper C++

- The engine is written in C style today (flat functions, structs, macros).
- Move it to real C++: classes that own their data, real inheritance where it fits, no more big macro tricks.
- Do it a little at a time, keep the engine working between steps.

## 9. Fetch dependencies automatically instead of bundling them

- What is still bundled in `code/third-party/`: fluidsynth, imgui, miniz,
  openal (the prebuilt Windows package: headers, import libraries and DLLs),
  qclib, stb.
- The build should download the dependencies it needs and compile them itself, no vendored copies lying around.
- Keep the downloads pinned to known versions so builds stay the same over time.
- Done so far: running scons on a non-Windows system fetches the pinned
  OpenAL Soft 1.23.1 and FluidSynth 2.6.1 sources into build/downloads/ before
  it stops (the build still targets Windows). timidy is gone, FluidSynth
  replaced it.

## 10. Replace the heavy dependencies with stb

- Replace the heavy third-party libraries with stb headers where they can do the same job.
- stb_image replaces jpeg-6 and libpng for image loading.
- stb_truetype replaces freetype (ft2) if the font rendering allows it.
- Only swap what can be swapped without losing features. Anything left stays on the auto-fetch list from task 9.
- Done: jpeg-6, libpng, zlib and ft2 are gone. stb_image decodes PNG/JPEG, stb_image_write
  encodes the JPEG screenshots, stb_truetype rasterizes the fonts and miniz backs the
  pk3 unzip code.

## 11. Implement QuakeC and drive the game code with it

- qclib is already vendored at `code/third-party/qclib` (the FTE QuakeC library).
- Make it work, then drive most of the game logic through QuakeC.
- QuakeC lives inside the cgame, game, ui and botlib modules only. It must never live inside the engine or be statically linked into it. Those modules load it dynamically.
- The engine stays native C++, the gameplay becomes QuakeC.

## 12. Open the x64 and Linux builds

- The engine builds and runs on Windows x86 only today.
- Get the engine to compile and run in 64-bit and on Linux, the game module
  ABI included.
- Turn warnings into errors in the build once every platform is warning-free.

## 13. Draw the menu and HUD into their own framebuffer

- The 2D pass (menus, HUD, console) renders straight into the screen
  framebuffer today.
- Render it into a separate framebuffer instead, then blit it over the 3D
  view. The renderer already has idGLFramebuffer for this.
- First step is a 1:1 blit behind a cvar so nothing changes by default.
  Scaling and other 2D post effects come after that.

## Priority order

1. Auto download and replacement of dependencies (tasks 9 and 10).
2. Botlib as a DLL with the parser in the engine (task 3).
3. Full C++ with idLib (tasks 5 and 8).
4. The x64 and Linux builds (task 12).
