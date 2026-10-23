# Notes

What I understand about this repository and the plan, saved for future reference.
Kept in plain English on purpose.

## What the repository is today

The engine is the Return to Castle Wolfenstein branch of id Tech 3. On top of it
sits a port of classic Doom (by Justin Marshall): a modified q3map (now DoomMap)
reads Doom dwd map files and builds Quake 3 BSPs, and the game boots into a
shareware demo mode until a doom.wad is placed in base/wad/.

The build is SCons (task 1). It builds on Windows only for now (task 4 opens
other systems), with clang-cl when it is in the PATH and plain MSVC as the
fallback, and 32-bit only: the game module ABI is still 32-bit until task 4
does the 64-bit work (the old bytecode VM that also forced this is gone,
task 2 removed it). The engine executable is Doom.exe (the
old CMake target was "wolf"). Everything the build makes lands in bin/ at the
root, and the game modules are also copied into base/, where the engine loads
them.

## Folder map

- base/ - game data (maps, wads, libraries loaded by the engine)
- code/ - the engine and the game. The plan calls this src/, the rename is still open.
- code/game/ - game, cgame, ui and botlib
- code/idLib/ - idLib from Doom 3 BFG, already vendored
- code/third-party/ - bundled dependencies (imgui, miniz, openal, qclib, stb, timidy)
- references/DOOM-3-BFG - Doom 3 BFG source, used as reference
- references/doom3.gpl - Doom 3 GPL source, used for the audio system
- bin/ - the finished binaries (Doom.exe, the game modules, toolsx86.dll, tests)
- build/ - object files and generated files, one folder per config

## How the SCons build works

- SConstruct at the root, helpers in site_scons/doom_build.py, source lists in
  site_scons/doom_sources.py.
- The old CMake files had stale folder names (win32/, jpeg-6/, ft2/, zlib/,
  libpng/, timidy/, game/, cgame/, ui/, botlib/ written as top level folders).
  The source lists keep those names and fix_path() in doom_sources.py moves
  them to the real folders (sys/win32/, third-party/..., game/game/ and so on).
  The old CMake also had no include folders at all and pointed imgui at
  code/imgui instead of code/third-party/imgui, so the old build was broken
  anyway. The SCons build sets the include folders per module.
- The engine sources still include each other with the paths of the old
  folder layout, like "../game/q_shared.h". The folders moved but the
  includes were never rewritten, so the build adds a few extra include
  folders (LEGACY_INCLUDE_PATHS in site_scons/doom_build.py) that make
  the old include paths find the real files. No engine source needs
  changing for now.
- Every module (wolf, cgame, qagame, ui, renderer, botlib and the rest) gets
  its own copy of the code tree under build/<config>/<module>/code. This way
  the same file compiled with different settings per module never collides.
- Small files are written into build/<config>/generated while scons starts:
  OpenAL's config.h (made from config.h.in), version.h, default_hrtf.txt
  (the HRTF data as a hex list) and compiler-exports.def for the editor.
- miniz builds as a small static library (doom_miniz) from third-party/miniz
  and replaces the old zlib. The unzip code in qcommon uses its inflate API.
- Image and font work goes through the stb headers in third-party/stb:
  stb_image decodes PNG and JPEG, stb_image_write writes the JPEG screenshots
  and stb_truetype rasterizes the fonts. No jpeg-6, libpng, zlib or ft2 left.
- OpenAL 1.23.1 builds as a static library straight from third-party/openal,
  with the settings the old build forced: WASAPI, WinMM and Wave backends,
  SSE2 code generation for x86, the HRTF data built in.
- Useful commands: scons, scons config=debug, scons editor=no, scons tests,
  scons test, scons -c.

## How the game modules talk to the engine (after task 2)

- The Q3VM is gone: vm.cpp, vm_interpreted.cpp, vm_x86.cpp, vm_local.h,
  vm_args.h, the vmMain entry points, the syscall tables (g_syscalls.cpp,
  cg_syscalls.cpp, ui_syscalls.cpp) and bg_lib.cpp are all deleted.
- Each module exports one plain C entry point: GetGameAPI in qagamex86.dll,
  GetCGameAPI in cgamex86.dll, GetUIAPI in uix86.dll. The .def files list
  only that name.
- The engine builds a const import struct (gameImport_t, cgameImport_t,
  uiImport_t in sv_game.cpp, cl_cgame.cpp, cl_ui.cpp), one member per old
  trap_ function. Sys_LoadDll loads the DLL, GetProcAddress finds the entry
  point, the entry point is called with the import struct and returns the
  module's export struct.
- The module side keeps its trap_ functions (g_import.cpp, cg_import.cpp,
  ui_import.cpp), each one just forwards to the matching function pointer.
  Nothing else in the module code changed.
- The export structs (gameExport_t, cgameExport_t, uiExport_t) are defined in
  g_public.h, cg_public.h and ui_public.h and have one member per old vmMain
  command. The engine holds them in gameExport, cgameExport, uiExport.
- Restarting a module means: call its Shutdown, unload the DLL, load it from
  disk again. There is no state left inside the engine to reset.
- The UI module still checks apiVersion on load; game and cgame do not need
  it because the struct layout is fixed at build time now.

## What each task means in practice

1. SCons: the SConstruct is at the root and the CMake files are removed. The
   build still needs its first green run on CI, then it is closed.
2. Q3VM: done. The VM code is deleted and the modules use the import/export
   structs described above. The 32-bit lock from the VM is gone; what is
   left is the module ABI itself, which task 4 handles.
3. Botlib: code/game/botlib is a static library linked into the engine today. The
   parser side is l_script, l_precomp and l_struct (the l_ prefix files). Those
   move into the engine, everything else becomes a DLL the engine loads. The
   engine's bot import table (EA, AI functions) becomes the DLL boundary.
4. Warnings: gcc/clang on Linux, clang on Windows. Fix warnings for real, then
   switch the build to warnings-as-errors.
5. idLib: already vendored in code/idLib (BFG edition, GPLv3). id Software started
   hooking it into their RTCW branch and reverted it because they ran out of
   time, so the port is finishable from their known direction. Start with the
   base pieces (idStr, containers, math, files) and let engine code grow into it.
6. extractfuncs: nothing in the repository yet. In the old id / Raven tools it
   scans the game source and generates a source file the game DLL needs for save
   games. It has to be brought in and run as a build step before the game DLL.
7. Audio: the Doom 3 sound system from references/doom3.gpl (its OpenAL support is
   better than the BFG version). It lands in code/sound and replaces snd_openal,
   snd_wave, snd_stream, snd_music, snd_musicmix and the opl code over time.
8. C++: flat C functions and structs become classes that own their data, with
   inheritance where it actually fits. Step by step, not one big rewrite.
9. Dependencies: stop bundling. The build downloads pinned versions and compiles
   them. Where to fetch from and which versions should be listed in one place in
   the SCons build.
10. stb: stb_image replaces jpeg-6 and libpng for image loading, stb_truetype can
    replace freetype (ft2) if the font code allows it. stb is public domain / MIT,
    so it mixes with the GPL engine without problems.
11. QuakeC: code/third-party/qclib is the FTE QuakeC library. It gets implemented
    and then drives cgame, game, ui and botlib. It only lives inside those
    modules and is loaded dynamically, never inside the engine.

## Licenses, short version

- Engine (RTCW / ET code) and references/doom3.gpl: GPL.
- idLib (Doom 3 BFG): GPLv3.
- qclib (FTE QuakeWorld): GPL, check the exact version text before shipping.
- stb headers: public domain / MIT.
Not legal advice, but nothing here obviously clashes. Double check qclib.

## Open questions

- Rename code/ to src/ to match the plan, or keep code/ and update the docs?
- extractfuncs: pull the original tool from the RTCW / ET sources in as reference
  and adapt it, or write a fresh one?

