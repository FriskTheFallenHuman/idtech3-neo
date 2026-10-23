# COPILOT.md

Working notes for AI assistants in this repository. This file holds the same
information as CLAUDE.md. Keep both in sync when the plan changes.

## What this project is

A new source port. The goal: get Quake 3 and Return to Castle Wolfenstein
running on the id Tech 3 branch that Enemy Territory uses.

## The plan

1. Replace the CMake build with SCons. The binaries go to `bin/` at the root of
   the repository.
2. Remove the Q3VM code from the engine and the game code, turn it into simple
   C import/export.
3. Refactor botlib into a DLL that the engine loads. The parser side of botlib
   lives in the engine (it is a parser, that is the point of it).
4. Get the engine to compile and run on both x64 and x86 and fix all warnings
   that exist.
5. Port Doom 3's idLib. id Software was doing this on their RTCW branch but
   reverted it because they ran out of time. The BFG edition idLib is vendored
   in the engine folder.
6. Make extractfuncs work and run it as a pre-step for the game DLL.
7. Port the full Doom 3 audio system from references/doom3.gpl, that version has
   the better OpenAL support.
8. Replace the C style code with proper C++ and inheritance.
9. Fetch the dependencies automatically and compile them instead of bundling
   them.
10. Replace the heavy dependencies with stb where possible.
11. Implement QuakeC (vendored in third-party/qclib) and drive most of the game
    code through it. It must work for cgame, game, ui and botlib modules, and it
    must only live inside those modules or be loaded dynamically.

## Repository layout

- src/ (today: code/) - the engine itself
- src/game/ (today: code/game/) - the game itself
- src/third-party/ (today: code/third-party/) - third-party dependencies, new
  ones go here
- references/ - reference sources needed for each task

## Working rules

- Commit titles and descriptions stay short, clear and in plain English.
- Keep TODO.md (the full task list) and PROGRESS.md (what is being worked on,
  in progress and planned) up to date.
- Docs and task descriptions in plain English, common words, no over-engineered
  vocabulary.
- NOTES.md holds the deeper understanding of the repository and each task.
- New third-party code goes into the third-party folder, and mention possible
  license conflicts (GPLv3 and friends) when bringing code in.

## Priority order

1. GitHub auto builds + SCons.
2. Auto download and replacement of dependencies.
3. Removal of C style code, full C++ plus idLib (GPLv3 compatible).
