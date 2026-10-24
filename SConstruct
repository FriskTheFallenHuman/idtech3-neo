# Build file for the whole project. Run "scons" from the repository root.
#
#   scons               build the engine, the game modules and the editor
#   scons config=debug  build the debug version (release is the default)
#   scons tests         build the test programs into bin/tests
#   scons test          build and run the tests
#   scons -c            remove the build files
#
# Everything the build produces lands in bin/ at the repository root, except
# the game modules: they are built into base/ itself, the game data folder,
# because that is where the engine loads them from. bin/base is a link to
# base/, so the engine finds them next to the game data.

import os
import shutil
import stat
import subprocess

import doom_build
import doom_sources

# --- options ---

config = ARGUMENTS.get('config', 'release')
arch = ARGUMENTS.get('arch', 'x86')
build_editor = ARGUMENTS.get('editor', 'yes') != 'no'

if os.name != 'nt':
    # The build itself still targets Windows, but fetch the OpenAL Soft and
    # FluidSynth sources now so they are ready for the non-Windows port.
    try:
        doom_build.fetch_openal_soft(os.path.join('build', 'downloads'))
        print('OpenAL Soft sources were fetched into build/downloads/,')
        print('see build/downloads/README-openal-soft.txt.')
    except Exception as error:
        print('Could not fetch OpenAL Soft sources: %s' % error)
    try:
        doom_build.fetch_fluidsynth(os.path.join('build', 'downloads'))
        print('FluidSynth sources were fetched into build/downloads/,')
        print('see build/downloads/README-fluidsynth.txt.')
    except Exception as error:
        print('Could not fetch FluidSynth sources: %s' % error)
    print('This port builds on Windows only for now.')
    print('Building on other systems is planned, see task 12 in TODO.md.')
    Exit(1)

if arch != 'x86':
    print('Only x86 builds work right now. The game module ABI is still')
    print('32-bit. Task 12 in TODO.md opens the door to 64-bit.')
    Exit(1)

if config not in ('debug', 'release'):
    print('Unknown config "%s". Use config=debug or config=release.' % config)
    Exit(1)

# --- the base environment ---

# build/<config>/ holds the object files, one folder per module, because the
# modules compile some of the same files with different settings.
build_root = os.path.join('build', config)
build_target = '#build/' + config

env = doom_build.make_base_env(config)
doom_build.write_generated_files(build_root)
generated_target = build_target + '/generated'


def module_env(name):
    """A copy of the base environment with its own code/ mirror.

    Every module gets its own mirror so files that are compiled by more than
    one module never fight over the same object file.
    """
    e = env.Clone()
    variant = build_target + '/' + name + '/code'
    e.VariantDir(variant, '#code', duplicate=0)
    return e, variant


# --- bin/base links to the game data ---

# The game data lives in base/ at the repository root and the engine loads
# the game modules from the base directory next to the executable. bin/base
# is a link to base/ so both are the same folder: the modules are built
# straight into the game data, the way the old build installed them.
# Windows needs a directory junction for this, other systems a plain
# symlink.
bin_base = os.path.join('bin', 'base')
if os.path.isdir(bin_base) and not os.path.islink(bin_base):
    if getattr(os.lstat(bin_base), 'st_reparse_tag', 0) != stat.IO_REPARSE_TAG_MOUNT_POINT:
        # An old build left a real folder here. It only holds build copies,
        # so replacing it with the link is safe.
        shutil.rmtree(bin_base)
if not os.path.exists(bin_base):
    os.makedirs('bin', exist_ok=True)
    if os.name == 'nt':
        subprocess.check_call(
            'mklink /J "%s" "%s"' % (bin_base, os.path.abspath('base')),
            shell=True)
    else:
        os.symlink('base', bin_base)


# --- the small support libraries ---

# miniz replaces the old bundled zlib (and the libpng build that sat on top
# of it). One C file, no configuration needed.
miniz_env, miniz_variant = module_env('doom_miniz')
miniz_env.Append(CPPPATH=['#code/third-party/miniz'])
doom_miniz = miniz_env.StaticLibrary(
    build_target + '/doom_miniz',
    [miniz_variant + '/' + p for p in doom_sources.MINIZ_SOURCES])

opl_env, opl_variant = module_env('doom_opl')
doom_opl = opl_env.StaticLibrary(
    build_target + '/doom_opl',
    [opl_variant + '/' + p for p in doom_sources.OPL_SOURCES])

# OpenAL Soft: the bundled copy is the prebuilt Windows package, so the
# engine links its import library and loads the matching DLL at runtime.
# Non-Windows systems fetch the sources instead, see fetch_openal_soft()
# in doom_build.py.
OpenAL32 = env.File('#code/third-party/openal/libs/Win32/OpenAL32.lib')

# SDL3: the window, GL context and input all go through the SDL3 library
# that ships with the engine, so the import library is linked here and the
# matching DLL is copied next to the engine and the tests below.
SDL3 = env.File('#code/third-party/SDL3/lib/x86/SDL3.lib')

# --- the engine parts ---

renderer_env, renderer_variant = module_env('renderer')
renderer_env.Append(
    CPPDEFINES=['__USEA3D', '__A3D_GEOM'],
    CPPPATH=[
        '#code', '#code/third-party/stb', '#code/third-party/miniz',
        generated_target])
renderer = renderer_env.StaticLibrary(
    build_target + '/renderer',
    [renderer_variant + '/' + p for p in
     doom_sources.RENDERER_SOURCES + doom_sources.RENDERER_EXTRA_SOURCES])

botlib_env, botlib_variant = module_env('botlib')
botlib_env.Append(CPPDEFINES=['BOTLIB'])
botlib = botlib_env.StaticLibrary(
    build_target + '/botlib',
    [botlib_variant + '/' + p for p in doom_sources.BOTLIB_SOURCES])

splines_env, splines_variant = module_env('splines')
splines = splines_env.StaticLibrary(
    build_target + '/splines',
    [splines_variant + '/' + p for p in doom_sources.SPLINES_SOURCES])

# --- the editor parts ---

editor_core_env, editor_core_variant = module_env('doomed_core')
editor_core_env.Append(
    CPPDEFINES=['WIN32_LEAN_AND_MEAN', 'WINVER=0x0A00', '_WIN32_WINNT=0x0A00'],
    CPPPATH=['#code/tools/editor', '#code'])
doomed_core = editor_core_env.StaticLibrary(
    build_target + '/doomed_core',
    [editor_core_variant + '/' + p for p in doom_sources.EDITOR_CORE_SOURCES])

imgui_env, imgui_variant = module_env('doomed_imgui')
imgui_env.Append(
    CPPDEFINES=['WINVER=0x0A00', '_WIN32_WINNT=0x0A00'],
    CPPPATH=['#code/third-party/imgui', '#code/third-party/imgui/backends',
             '#code/third-party/SDL3/include'])
doomed_imgui = imgui_env.StaticLibrary(
    build_target + '/doomed_imgui',
    [imgui_variant + '/' + p for p in doom_sources.IMGUI_SOURCES])

dmap_env, dmap_variant = module_env('dmap')
dmap_env.Append(
    CPPPATH=['#code', '#code/third-party/stb'])
dmap = dmap_env.StaticLibrary(
    build_target + '/dmap',
    [dmap_variant + '/' + p for p in doom_sources.DMAP_SOURCES])

aasbuild_env, aasbuild_variant = module_env('aasbuild')
aasbuild_env.Append(
    CPPDEFINES=['BSPC', 'strcasecmp=_stricmp'],
    CPPPATH=['#code', '#code/third-party/miniz'])
aasbuild = aasbuild_env.StaticLibrary(
    build_target + '/aasbuild',
    [aasbuild_variant + '/' + p for p in doom_sources.AASBUILD_SOURCES])

# The editor DLL: every .cpp in tools/editor that is not part of the core
# library and not on the exclude list.
editor_dll = None
aas_copies = []
if build_editor:
    editor_cpp = []
    for name in sorted(os.listdir(os.path.join('code', 'tools', 'editor'))):
        if not name.endswith('.cpp'):
            continue
        path = 'tools/editor/' + name
        if path in doom_sources.EDITOR_EXCLUDED_SOURCES:
            continue
        editor_cpp.append(path)

    editor_env, editor_variant = module_env('editor')
    editor_env.Append(
        CCFLAGS=['/utf-8'],
        CPPDEFINES=['WIN32_LEAN_AND_MEAN', 'WINVER=0x0A00', '_WIN32_WINNT=0x0A00'],
        CPPPATH=['#code/tools/editor', '#code', '#code/third-party/imgui',
                 '#code/third-party/imgui/backends', '#code/third-party/SDL3/include',
                 generated_target])
    editor_sources = [editor_variant + '/' + p for p in
                      editor_cpp + doom_sources.EDITOR_EXTRA_SOURCES]
    editor_sources.append(generated_target + '/compiler-exports.def')
    editor_dll = editor_env.SharedLibrary(
        '#bin/toolsx86',
        editor_sources,
        LIBS=[doomed_core, doomed_imgui, dmap, aasbuild, doom_miniz, SDL3,
              'comdlg32', 'shell32', 'advapi32', 'windowscodecs', 'ole32',
              'dwmapi', 'winmm'])

    # The navigation profiles are plain data files the game reads from
    # base/aas, like the old build installed them.
    aas_copies = env.Install(
        '#base/aas', ['#code/' + cfg for cfg in doom_sources.AAS_CONFIG_FILES])

# --- the game modules ---

# The modules are built straight into base/ through the bin/base link, where
# the engine loads them from. The engine also loads them from mod folders
# set with fs_game, so the same files work for mods that ship their own.
cgame_env, cgame_variant = module_env('cgame')
cgame_env.Append(CPPDEFINES=['CGAMEDLL'], CPPPATH=['#code', generated_target])
cgame = cgame_env.SharedLibrary(
    '#bin/base/cgamex86',
    [cgame_variant + '/' + p for p in
     doom_sources.CGAME_SOURCES + doom_sources.CGAME_EXTRA_SOURCES
     + doom_sources.DOOM_FX_SOURCES])

qagame_env, qagame_variant = module_env('qagame')
qagame_env.Append(
    CPPDEFINES=['GAMEDLL', 'DOOM_SAVE_CALLBACKS'],
    CPPPATH=['#code', generated_target])
qagame = qagame_env.SharedLibrary(
    '#bin/base/qagamex86',
    [qagame_variant + '/' + p for p in
     doom_sources.GAME_SOURCES + doom_sources.GAME_EXTRA_SOURCES],
    LIBS=['winmm'])

ui_env, ui_variant = module_env('ui')
ui_env.Append(CPPDEFINES=['UI_EXPORTS'], CPPPATH=['#code', generated_target])
ui_dll = ui_env.SharedLibrary(
    '#bin/base/uix86',
    [ui_variant + '/' + p for p in
     doom_sources.UI_SOURCES + doom_sources.UI_EXTRA_SOURCES])

# --- the engine executable ---

wolf_env, wolf_variant = module_env('wolf')
wolf_env.Append(
    CPPDEFINES=['_WINDOWS', '__USEA3D', '__A3D_GEOM'],
    CPPPATH=[
        '#code',
        '#code/third-party/openal/include',
        '#code/third-party/fluidsynth/include',
        '#code/third-party/stb',
        '#code/third-party/miniz',
        generated_target])
wolf_env['SUBSYSTEM'] = 'WINDOWS'
# SCons hands file types it does not know to the linker, and the linker
# cannot read a .rc file. Compile the resource file with the resource
# compiler first and feed the .res file to the Program call.
wolf_res = wolf_env.RES(
    wolf_variant + '/sys/rc/winquake.res',
    wolf_variant + '/sys/rc/winquake.rc')
wolf = wolf_env.Program(
    '#bin/Doom',
    [wolf_variant + '/' + p for p in
     doom_sources.WOLF_SOURCES + doom_sources.WOLF_EXTRA_SOURCES] + [wolf_res],
    LIBS=[renderer, botlib, splines, OpenAL32, SDL3, doom_opl,
          doom_miniz,
          'winmm', 'ole32', 'oleaut32', 'wsock32', 'user32', 'gdi32', 'shell32', 'advapi32'],
    # SDL provides WinMain inside SDL3.dll now that the engine entry point is
    # SDL_main, so the windows subsystem has to be picked explicitly
    LINKFLAGS=['/STACK:8388608', '/LARGEADDRESSAWARE', '/SUBSYSTEM:WINDOWS'])

# The engine links the SDL3 import library, so the DLL that matches it is
# copied next to the engine and the tests. libfluidsynth-3.dll needs the
# same SDL3.dll next to it, and the copy below satisfies that too.
sdl_copies = [
    env.InstallAs('#bin/SDL3.dll', '#code/third-party/SDL3/lib/x86/SDL3.dll'),
    env.InstallAs('#bin/tests/SDL3.dll', '#code/third-party/SDL3/lib/x86/SDL3.dll'),
]

# The import library binds to OpenAL32.dll, so the bundled soft_oal.dll is
# copied next to the engine and the tests under the name the linker expects.
openal_copies = [
    env.InstallAs('#bin/OpenAL32.dll', '#code/third-party/openal/bin/Win32/soft_oal.dll'),
    env.InstallAs('#bin/tests/OpenAL32.dll', '#code/third-party/openal/bin/Win32/soft_oal.dll'),
]

# FluidSynth is loaded at runtime, so its DLL ships next to the engine and
# the tests when the prebuilt package is present. libfluidsynth-3.dll needs
# sndfile.dll and SDL3.dll next to it; SDL3.dll itself is the engine copy
# installed above. The library itself is optional: the engine still boots
# without it.
fluidsynth_bin = 'code/third-party/fluidsynth/Win32/bin'
fluidsynth_copies = []
if os.path.exists(os.path.join(fluidsynth_bin, 'libfluidsynth-3.dll')):
    for dll in ('libfluidsynth-3.dll', 'sndfile.dll'):
        source = os.path.join(fluidsynth_bin, dll)
        if os.path.exists(source):
            fluidsynth_copies.append(
                env.InstallAs('#bin/' + dll, '#' + source))
            fluidsynth_copies.append(
                env.InstallAs('#bin/tests/' + dll, '#' + source))

# The engine is not finished until the modules are built too.
engine_extra = [cgame, qagame, ui_dll]
if build_editor:
    engine_extra += [editor_dll] + aas_copies
env.Depends(wolf, engine_extra)

# --- the tests ---

BUILT_LIBS = {
    'doom_miniz': doom_miniz,
    'doom_opl': doom_opl,
    'OpenAL': OpenAL32,
    'doomed_core': doomed_core,
    'doomed_imgui': doomed_imgui,
}

test_include_paths = [
    '#code',
    '#code/tools/editor',
    '#code/third-party/openal/include',
    '#code/third-party/fluidsynth/include',
    '#code/third-party/stb',
    '#code/third-party/miniz',
    '#code/third-party/imgui',
    '#code/third-party/imgui/backends',
    '#code/third-party/SDL3/include',
    generated_target]

test_nodes = []
for test in doom_sources.TESTS:
    if test['name'].startswith('doomed_') and not build_editor:
        continue

    test_env, test_variant = module_env('tests/' + test['name'])
    test_env.Append(CPPPATH=test_include_paths)
    if test.get('defines'):
        test_env.Append(CPPDEFINES=test['defines'])
    if test.get('sections'):
        test_env.Append(CCFLAGS=['/Gy'],
                        LINKFLAGS=['/OPT:REF', '/INCREMENTAL:NO'])
    if test['name'] == 'doomed_host_tests':
        test_env.Append(LINKFLAGS=['/LARGEADDRESSAWARE'])

    test_libs = [BUILT_LIBS[name] for name in test.get('libs', [])]
    test_libs += test.get('syslibs', [])
    # The host test drives the editor through SDL windows and events, so it
    # links the SDL3 import library itself. This has to sit in the builder's
    # LIBS list because that argument overrides the environment LIBS.
    if test['name'] == 'doomed_host_tests':
        test_libs += [SDL3]
    # Test files live in test/ at the repository root, engine files they
    # pull in still live under code/.
    test_sources = []
    for path in doom_sources.fix_paths(test['sources']):
        if path.startswith('test/'):
            test_sources.append('#' + path)
        else:
            test_sources.append(test_variant + '/' + path)
    node = test_env.Program(
        '#bin/tests/' + test['name'],
        test_sources,
        LIBS=test_libs)
    if test['name'] == 'doomed_host_tests' and editor_dll:
        env.Depends(node, editor_dll)
    test_nodes.append(node)

env.Alias('tests', test_nodes)

# "scons test" builds the tests, runs them and stops when one fails.
# The tests stay optional for a plain build; the CI run always executes them.
env['TEST_ARGS'] = {test['name']: test.get('args', [])
                    for test in doom_sources.TESTS}
test_run = env.Command(
    build_target + '/tests-run.stamp',
    test_nodes,
    doom_build.run_tests)
env.AlwaysBuild(test_run)
env.Alias('test', test_run)

# --- the default target ---

default_targets = [wolf] + sdl_copies + openal_copies + fluidsynth_copies
if build_editor:
    default_targets += [editor_dll] + aas_copies
env.Default(default_targets)

Help("""
Targets:
  scons              build the engine, the game modules and the editor
  scons tests        build the test programs into bin/tests
  scons test         build and run the tests
  scons -c           remove the build files

Options:
  config=debug|release   release is the default
  arch=x86               only x86 builds work today
  editor=yes|no          yes is the default
""")

print('Config: %s, arch: %s, editor: %s' % (config, arch, build_editor))
print('Binaries go to bin/, the build files live in build/%s/' % config)
print('The game modules go to base/, bin/base links to it.')
