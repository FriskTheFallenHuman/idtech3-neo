# Notes

What I understand about this repository and the plan, saved for future reference.
Kept in plain English on purpose.

## What the repository is today

The engine is the Return to Castle Wolfenstein branch of id Tech 3. On top of it
sits a port of classic Doom (by Justin Marshall): a modified q3map (now DoomMap)
reads Doom dwd map files and builds Quake 3 BSPs, and the game boots into a
shareware demo mode until a doom.wad is placed in base/wad/.

The build is SCons (task 1). It builds on Windows only for now (task 12 opens
other systems), with clang-cl when it is in the PATH and plain MSVC as the
fallback, and 32-bit only: the game module ABI is still 32-bit until task 12
does the 64-bit work (the old bytecode VM that also forced this is gone,
task 2 removed it). The engine executable is Doom.exe (the
old CMake target was "wolf"). Everything the build makes lands in bin/ at the
root, except the game modules: they are built into base/ itself, the game
data folder, and the aas profiles go to base/aas. bin/base is a link to
base/, so the engine loads the modules from the exe folder's base directory
and from mod folders set with fs_game.

## Folder map

- base/ - game data (maps, wads) plus the game modules and the aas profiles
  the build puts there; bin/base links to this folder
- bin/ - the finished binaries (Doom.exe, toolsx86.dll, tests)
- code/ - the engine and the game. The plan calls this src/, the rename is still open.
- code/game/ - game, cgame, ui and botlib
- code/idLib/ - idLib from Doom 3 BFG, already vendored
- code/third-party/ - bundled dependencies (fluidsynth, imgui, miniz, openal, qclib, stb)
- references/DOOM-3-BFG - Doom 3 BFG source, used as reference
- references/doom3.gpl - Doom 3 GPL source, used for the audio system
- build/ - object files and generated files, one folder per config

## How the SCons build works

- SConstruct at the root, helpers in site_scons/doom_build.py, source lists in
  site_scons/doom_sources.py.
- bin/base is a link to the repository base/ folder (a directory junction on
  Windows, a symlink elsewhere). The build creates it when it is missing and
  replaces a real folder left by an older build. The game modules are built
  straight into base/ through this link, so no module copies sit in bin/,
  and the aas profiles are installed into base/aas like the old build did.
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
  compiler-exports.def for the editor is the only one left.
- miniz builds as a small static library (doom_miniz) from third-party/miniz
  and replaces the old zlib. The unzip code in qcommon uses its inflate API.
- Image and font work goes through the stb headers in third-party/stb:
  stb_image decodes PNG and JPEG, stb_image_write writes the JPEG screenshots
  and stb_truetype rasterizes the fonts. No jpeg-6, libpng, zlib or ft2 left.
- OpenAL Soft: third-party/openal is the prebuilt Windows package. The engine
  links its import library (libs/Win32/OpenAL32.lib) and loads the bundled
  soft_oal.dll, which scons copies next to the binaries as OpenAL32.dll.
  Non-Windows systems fetch the OpenAL Soft 1.23.1 sources into
  build/downloads/ instead, see task 9.
- FluidSynth: third-party/fluidsynth ships the 2.6.1 headers, the CMake and
  pkgconfig files and the prebuilt Win32 DLLs (libfluidsynth-3.dll with its
  sndfile.dll and SDL3.dll dependencies). The engine links no import
  library; snd_fluidsynth.cpp loads libfluidsynth-3.dll at runtime with
  LoadLibrary and resolves the entry points with GetProcAddress, the same
  pattern as the optional OpenAL extension entry points. scons copies the
  three DLLs next to the engine and the tests. Missing DLLs mean no synth:
  the engine still boots and plays OPL2 and PCM music. Non-Windows systems
  fetch the 2.6.1 sources into build/downloads/ like OpenAL Soft.
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
- Sys_LoadDll searches in order: the working directory, the exe folder's
  base directory (bin/base, a link to the repository base/ folder), then
  fs_basepath and fs_cdpath with the game dir, so mod folders set with
  fs_game work too. The build puts the modules into base/.

## How the music playback works (after the FluidSynth swap)

- TiMidity is gone. snd_music.cpp drives FluidSynth through the runtime
  loader in snd_fluidsynth.cpp.
- The SoundFont is base/sound/timidity/TimGM6mb.sf2, read from fs_basepath.
- The player timing source is "sample", so rendering audio pulls the MIDI
  timeline along; the streaming logic keeps working the way it did with
  TiMidity's offline rendering.
- s_musicBackend picks the backend: "opl" (default) plays the original OPL2
  GENMIDI tunes, "fluidsynth" plays the MIDI files through FluidSynth.
  WAV-like PCM tracks decode the same way on both backends.

## How the Doom automap and the weapon sprite work

- The automap rasterizes a 256x256 pixel buffer in cgame (cg_doommap.cpp)
  and hands it to the engine with trap_R_DrawDoomMap(x, y, width, height,
  pixels), which goes to re.DrawStretchRaw. The map fills the whole view
  area above the status bar and keeps Doom's 1.2 horizontal stretch.
- CG_DoomMapVisible() tells the rest of the cgame when the map is up.
  The weapon sprite is not drawn then, like the original game.
- The weapon sprite is tinted with the sector light at the player
  position (trap_R_LightForPoint in CG_DrawDoomWeapon), so it is no
  longer full bright. The invisibility powerup multiplies the tint by
  0.3 and uses 0.35 alpha, the old shadow values.
- Monster, item and effect sprites were already lit through the Doom
  sector light (R_LightDoomPoly); the weapon was the one full bright
  sprite.

## Menu and HUD framebuffer (planned, task 13)

- Today the 2D elements draw straight into the screen framebuffer.
- The plan: draw the 2D pass into its own framebuffer (the renderer
  already has idGLFramebuffer, used by the VFX, HBAO and the editor)
  and blit it over the 3D view. That opens the door to scaling and other
  2D post effects later.
- First step: a screen-size framebuffer with a 1:1 blit behind a cvar,
  so the default look does not change.

## What each task means in practice

1. SCons: done. The SConstruct is at the root, the CMake files are removed
   and CI builds Windows x86. The Linux and x64 parts live in task 12.
2. Q3VM: done. The VM code is deleted and the modules use the import/export
   structs described above. The 32-bit lock from the VM is gone; what is
   left is the module ABI itself, which task 12 handles.
3. Botlib: code/game/botlib is a static library linked into the engine today. The
   parser side is l_script, l_precomp and l_struct (the l_ prefix files). Those
   move into the engine, everything else becomes a DLL the engine loads. The
   engine's bot import table (EA, AI functions) becomes the DLL boundary.
4. Warnings: done on Windows x86, the build is warning-free with clang-cl
   and CI keeps it that way. The Linux side and the warnings-as-errors
   switch are task 12.
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
11. QuakeC: code/third-party/qclib is the FTE QuakeWorld library. It gets implemented
    and then drives cgame, game, ui and botlib. It only lives inside those
    modules and is loaded dynamically, never inside the engine.
12. x64 and Linux: the engine compiles and runs on Windows x86 only today. This
    task opens the 64-bit builds and the Linux builds, the game module ABI
    included, and turns warnings into errors once every platform is clean.

## Licenses, short version

- Engine (RTCW / ET code) and references/doom3.gpl: GPL.
- idLib (Doom 3 BFG): GPLv3.
- qclib (FTE QuakeWorld): GPL, check the exact version text before shipping.
- FluidSynth: LGPL 2.1. It is loaded at runtime as a separate DLL and the
  headers are not compiled into the engine, which is the intended way to
  combine it with a GPL program.
- stb headers: public domain / MIT.
Not legal advice, but nothing here obviously clashes. Double check qclib.

## Open questions

- Rename code/ to src/ to match the plan, or keep code/ and update the docs?
- extractfuncs: pull the original tool from the RTCW / ET sources in as reference
  and adapt it, or write a fresh one?
