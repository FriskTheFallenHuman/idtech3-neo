# Progress

What is being worked on right now, what is in progress and what is planned.
Plain English only. Update this file as work moves forward.

## Working on right now

- Task 1 (SCons build): the SCons build is in and the CMake files are gone.
  CI runs it on Windows x86 and saves build.log. The CI log showed the old
  include paths (fixed with legacy include folders), broken copies into
  base/ (fixed with the SCons Install builder) and editor code using throw
  with exceptions off (fixed with /EHsc). Keep reading the log digest and
  fixing errors until the build is green.
- Task 2 (Q3VM removal): the code change is done and pushed. The engine and
  the game modules now talk through plain C import and export tables. The
  first CI run after the change will show whether it compiles clean.
- Tasks 9 and 10 (dependency replacement): jpeg-6, libpng, zlib and ft2 are
  out of third-party. stb_image decodes PNG and JPEG in the renderer and in
  the dmap WAD tools, stb_image_write encodes the JPEG screenshots,
  stb_truetype rasterizes the fonts and miniz (doom_miniz) backs the pk3
  unzip code. Running scons on a non-Windows system fetches the pinned
  OpenAL Soft 1.23.1 sources into build/downloads/ first. Waiting on CI to
  prove the build green after the swap.

## In progress

- Task 1 (SCons build + GitHub auto builds):
  - SConstruct at the root, helper code in site_scons/, source lists moved
    over from the old CMake files with the folder paths fixed.
  - Binaries land in bin/ at the root: Doom.exe, cgamex86.dll, qagamex86.dll,
    uix86.dll, toolsx86.dll, and the tests in bin/tests/.
  - The game modules are copied into base/, where the engine loads them.
  - OpenAL 1.23.1 builds natively from third-party/openal, no CMake anywhere
    in the build.
  - clang-cl is the preferred compiler, plain MSVC is the fallback. Windows
    x86 only for now (the game module ABI is still 32-bit, see task 4).
  - "scons tests" builds the tests, "scons test" runs them.
  - What is missing: a green build after the Q3VM removal.

- Task 2 (remove Q3VM, plain C import/export):
  - The VM files are deleted: qcommon/vm.cpp, vm_interpreted.cpp, vm_x86.cpp,
    vm_local.h, vm_args.h and the syscall tables (g_syscalls.cpp,
    cg_syscalls.cpp, ui_syscalls.cpp) plus bg_lib.cpp.
  - The modules export one entry point each: GetGameAPI, GetCGameAPI and
    GetUIAPI. The engine fills an import struct and gets the export struct
    back. No bytecode, no integer-encoded pointers, no enum-based calls.
  - Restarting a module now means: unload the DLL and load it again.

## Planned next, in priority order

1. Get the CI build green (tasks 1 and 2, see above).
2. Auto download and replacement of dependencies (tasks 9 and 10).
3. Botlib as a DLL with the parser in the engine (task 3).
4. Full C++ with idLib (tasks 5 and 8).

## Not started yet

- Task 3 (botlib as a DLL, parser into the engine)
- Task 6 (extractfuncs as a pre-step for the game DLL)
- Task 7 (Doom 3 audio system)
- Task 11 (QuakeC driving the game modules)

## Done

- Project planning docs (TODO.md, PROGRESS.md, NOTES.md, CLAUDE.md, COPILOT.md).
- GitHub workflow that builds the project and saves build.log.
- GitHub workflow that turns a build.log from the repo root into a short report.
- VSCode setup for building, running, debugging and testing the engine.
- SCons build replaces CMake (task 1, waiting on the first CI run to prove it).

