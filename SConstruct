# Build file for the whole project. Run "scons" from the repository root.
#
#   scons               build the engine, the game modules and the editor
#   scons config=debug  build the debug version (release is the default)
#   scons tests         build the test programs into bin/tests
#   scons test          build and run the tests
#   scons -c            remove the build files
#
# Everything the build produces lands in bin/ at the repository root. The
# engine loads the game modules from base/, so they are copied there too.

import os

import doom_build
import doom_sources

# --- options ---

config = ARGUMENTS.get('config', 'release')
arch = ARGUMENTS.get('arch', 'x86')
build_editor = ARGUMENTS.get('editor', 'yes') != 'no'

if os.name != 'nt':
    # The build itself still targets Windows, but fetch the OpenAL Soft
    # sources now so they are ready for the non-Windows port (task 4).
    try:
        doom_build.fetch_openal_soft(os.path.join('build', 'downloads'))
        print('OpenAL Soft sources were fetched into build/downloads/,')
        print('see build/downloads/README-openal-soft.txt.')
    except Exception as error:
        print('Could not fetch OpenAL Soft sources: %s' % error)
    print('This port builds on Windows only for now.')
    print('Building on other systems is planned, see task 4 in TODO.md.')
    Exit(1)

if arch != 'x86':
    print('Only x86 builds work right now. The game module ABI is still')
    print('32-bit. Task 4 in TODO.md opens the door to 64-bit.')
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


# --- the small support libraries ---

# miniz replaces the old bundled zlib (and the libpng build that sat on top
# of it). One C file, no configuration needed.
miniz_env, miniz_variant = module_env('doom_miniz')
miniz_env.Append(CPPPATH=['#code/third-party/miniz'])
doom_miniz = miniz_env.StaticLibrary(
    build_target + '/doom_miniz',
    [miniz_variant + '/' + p for p in doom_sources.MINIZ_SOURCES])

timidity_env, timidity_variant = module_env('doom_timidity')
timidity_env.Append(
    CPPDEFINES=['TIMIDITY_BUILD', 'TIMIDITY_STATIC'],
    CPPPATH=['#code/third-party/timidy/src'])
doom_timidity = timidity_env.StaticLibrary(
    build_target + '/doom_timidity',
    [timidity_variant + '/' + p for p in doom_sources.TIMIDITY_SOURCES])

opl_env, opl_variant = module_env('doom_opl')
doom_opl = opl_env.StaticLibrary(
    build_target + '/doom_opl',
    [opl_variant + '/' + p for p in doom_sources.OPL_SOURCES])

# OpenAL Soft, built as a static library from the vendored sources, with the
# same settings the old build used: WASAPI, WinMM and Wave backends, SSE
# code generation, and the HRTF data built into the library.
openal_env, openal_variant = module_env('openal')
openal_env.Append(
    CPPDEFINES=[
        '_WIN32', 'NOMINMAX', 'RESTRICT=__restrict', '_CRT_SECURE_NO_WARNINGS',
        'AL_BUILD_LIBRARY', 'AL_ALEXT_PROTOTYPES',
        ('ALC_API', ''), ('AL_API', ''), 'AL_LIBTYPE_STATIC'],
    CPPPATH=[
        '#code/third-party/openal',
        '#code/third-party/openal/common',
        '#code/third-party/openal/include',
        generated_target],
    CCFLAGS=['/arch:SSE2', '/W4', '/w14640', '/wd4065', '/wd4127', '/wd4268',
             '/wd4324', '/wd5030', '/wd5051'],
    CXXFLAGS=['/std:c++17', '/permissive-'])
OpenAL = openal_env.StaticLibrary(
    build_target + '/OpenAL',
    [openal_variant + '/' + p for p in doom_sources.openal_sources()])

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
    CPPPATH=['#code/third-party/imgui', '#code/third-party/imgui/backends'])
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
                 '#code/third-party/imgui/backends', generated_target])
    editor_sources = [editor_variant + '/' + p for p in
                      editor_cpp + doom_sources.EDITOR_EXTRA_SOURCES]
    editor_sources.append(generated_target + '/compiler-exports.def')
    editor_dll = editor_env.SharedLibrary(
        '#bin/toolsx86',
        editor_sources,
        LIBS=[doomed_core, doomed_imgui, dmap, aasbuild, doom_miniz,
              'comdlg32', 'shell32', 'advapi32', 'windowscodecs', 'ole32',
              'dwmapi', 'winmm'])

    # The navigation profiles are plain data files the game reads from base/.
    aas_copies = env.Install(
        '#base/aas', ['#code/' + cfg for cfg in doom_sources.AAS_CONFIG_FILES])

# --- the game modules ---

cgame_env, cgame_variant = module_env('cgame')
cgame_env.Append(CPPDEFINES=['CGAMEDLL'], CPPPATH=['#code', generated_target])
cgame = cgame_env.SharedLibrary(
    '#bin/cgamex86',
    [cgame_variant + '/' + p for p in
     doom_sources.CGAME_SOURCES + doom_sources.CGAME_EXTRA_SOURCES
     + doom_sources.DOOM_FX_SOURCES])

qagame_env, qagame_variant = module_env('qagame')
qagame_env.Append(
    CPPDEFINES=['GAMEDLL', 'DOOM_SAVE_CALLBACKS'],
    CPPPATH=['#code', generated_target])
qagame = qagame_env.SharedLibrary(
    '#bin/qagamex86',
    [qagame_variant + '/' + p for p in
     doom_sources.GAME_SOURCES + doom_sources.GAME_EXTRA_SOURCES],
    LIBS=['winmm'])

ui_env, ui_variant = module_env('ui')
ui_env.Append(CPPDEFINES=['UI_EXPORTS'], CPPPATH=['#code', generated_target])
ui_dll = ui_env.SharedLibrary(
    '#bin/uix86',
    [ui_variant + '/' + p for p in
     doom_sources.UI_SOURCES + doom_sources.UI_EXTRA_SOURCES])

# The engine loads the game modules from base/, so they are copied there.
# The Program/SharedLibrary calls above return a list of nodes that also
# carries the .lib and .exp files built next to each dll, so only the .dll
# files are listed here.
base_copies = env.Install(
    '#base',
    [env.File('#bin/' + name + '.dll')
     for name in ('cgamex86', 'qagamex86', 'uix86')])

# --- the engine executable ---

wolf_env, wolf_variant = module_env('wolf')
wolf_env.Append(
    CPPDEFINES=['_WINDOWS', '__USEA3D', '__A3D_GEOM', 'AL_LIBTYPE_STATIC',
                'TIMIDITY_STATIC'],
    CPPPATH=[
        '#code',
        '#code/third-party/openal/include',
        '#code/third-party/timidy/src',
        '#code/third-party/stb',
        '#code/third-party/miniz',
        generated_target])
wolf_env['SUBSYSTEM'] = 'WINDOWS'
# SCons hands file types it does not know to the linker, and the linker
# cannot read a .rc file. Compile the resource file with the resource
# compiler first and feed the .res file to the Program call.
wolf_res = wolf_env.RES(
    wolf_variant + '/sys/win32/winquake.res',
    wolf_variant + '/sys/win32/winquake.rc')
wolf = wolf_env.Program(
    '#bin/Doom',
    [wolf_variant + '/' + p for p in
     doom_sources.WOLF_SOURCES + doom_sources.WOLF_EXTRA_SOURCES] + [wolf_res],
    LIBS=[renderer, botlib, splines, OpenAL, doom_timidity, doom_opl,
          doom_miniz,
          'winmm', 'ole32', 'oleaut32', 'wsock32', 'user32', 'gdi32', 'shell32', 'advapi32'],
    LINKFLAGS=['/STACK:8388608', '/LARGEADDRESSAWARE'])

# The engine is not finished until the modules are built too.
engine_extra = [cgame, qagame, ui_dll] + base_copies
if build_editor:
    engine_extra += [editor_dll] + aas_copies
env.Depends(wolf, engine_extra)

# --- the tests ---

BUILT_LIBS = {
    'doom_miniz': doom_miniz,
    'doom_timidity': doom_timidity,
    'doom_opl': doom_opl,
    'OpenAL': OpenAL,
    'doomed_core': doomed_core,
    'doomed_imgui': doomed_imgui,
}

test_include_paths = [
    '#code',
    '#code/tools/editor',
    '#code/third-party/openal/include',
    '#code/third-party/timidy/src',
    '#code/third-party/stb',
    '#code/third-party/miniz',
    '#code/third-party/imgui',
    '#code/third-party/imgui/backends',
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
    node = test_env.Program(
        '#bin/tests/' + test['name'],
        [test_variant + '/' + p
         for p in doom_sources.fix_paths(test['sources'])],
        LIBS=test_libs)
    if test['name'] == 'doomed_host_tests' and editor_dll:
        env.Depends(node, editor_dll)
    test_nodes.append(node)

env.Alias('tests', test_nodes)

# "scons test" builds the tests, runs them and stops when one fails.
test_run = env.Command(
    build_target + '/tests-run.stamp',
    test_nodes,
    doom_build.run_tests)
env.AlwaysBuild(test_run)
env.Alias('test', test_run)

# --- the default target ---

default_targets = [wolf] + base_copies
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


