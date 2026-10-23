# Build helpers shared by the SConstruct file: the base compiler settings and
# the small files the build writes while it sets itself up.

import os
import re
import shutil

# Warning and language settings the old build passed to Clang on Windows.
CLANG_FLAGS = [
    '/W3',
    '/clang:-ferror-limit=0',
    '/clang:-Wno-writable-strings',
    '/clang:-Wno-deprecated-declarations',
    '/clang:-Wno-microsoft-enum-value',
    '/clang:-fno-operator-names',
    '/clang:-fno-strict-aliasing',
    '/clang:-fwrapv',
]

# Definitions the old build set for the whole project.
GLOBAL_DEFINES = [
    'WIN32',
    'C_ONLY',
    '_USE_32BIT_TIME_T',
    '_CRT_SECURE_NO_WARNINGS',
    '_CRT_NONSTDC_NO_WARNINGS',
    'NOMINMAX',
]

# The engine sources include each other with the folder names of the old
# tree, where game, cgame, ui, botlib, win32 and the bundled libraries sat
# directly inside code/. The folders have moved (game/game, game/cgame,
# sys/win32, third-party/...), but the includes were never rewritten. These
# include folders make the old include paths find the real files again, so
# no engine source has to be touched.
LEGACY_INCLUDE_PATHS = [
    '#code/game',              # "game/x.h", "cgame/x.h", "ui/x.h", "botlib/x.h"
    '#code/game/cgame',        # "fx/x.h" from the cgame tests
    '#code/game/game',         # "../game/x.h", "../cgame/x.h", "../ui/x.h", ...
    '#code/game/game/tests',   # "../../game/x.h" from the aasbuild compiler
    '#code/client',            # "../qcommon/x.h", "../renderer/x.h", ... from moved files
    '#code/sys',               # "win32/x.h"
    '#code/sys/win32',         # "../win32/x.h"
    '#code/third-party',       # "timidy/src/x.h", "openal/x.h", ...
    '#code/third-party/zlib',  # "../timidy/src/x.h" from the sound code
]

# What the OpenAL part supports on a Windows x86 build. Anything listed in
# config.h.in but not here stays undefined.
OPENAL_CONFIG_DEFINES = [
    'ALSOFT_EMBED_HRTF_DATA',
    'HAVE__ALIGNED_MALLOC',
    'HAVE_SSE',
    'HAVE_SSE2',
    'HAVE_SSE3',
    'HAVE_SSE4_1',
    'HAVE_WASAPI',
    'HAVE_WINMM',
    'HAVE_WAVE',
    'HAVE_MALLOC_H',
    'HAVE_INTRIN_H',
    'HAVE_GUIDDEF_H',
    'HAVE_CPUID_INTRINSIC',
    'HAVE_SSE_INTRINSICS',
]


def make_base_env(config):
    """The starting point for every other environment in the build.

    The project builds with Clang through the MSVC command line (clang-cl).
    When clang-cl is missing, plain MSVC is used as a fallback.
    """
    from SCons.Script import Environment

    env = Environment(tools=['default'], TARGET_ARCH='x86')

    if shutil.which('clang-cl'):
        env['CC'] = 'clang-cl'
        env['CXX'] = 'clang-cl'
        # clang-cl follows the host by default, so point it at 32-bit.
        env['CCFLAGS'] = ['--target=i686-pc-windows-msvc'] + CLANG_FLAGS
    else:
        print('clang-cl was not found, using the MSVC compiler instead.')
        print('The project normally builds with Clang, so expect problems.')
        # cl prints its banner for every file it compiles; /nologo keeps
        # the fallback build output readable.
        env['CCFLAGS'] = ['/nologo']

    # /EHsc turns C++ exceptions on: the editor format readers use throw.
    env['CXXFLAGS'] = ['/std:c++17', '/EHsc']
    env['SHLIBPREFIX'] = ''
    env.Append(CPPDEFINES=GLOBAL_DEFINES)
    # Include folders that make the old include paths work, see above.
    env.Append(CPPPATH=LEGACY_INCLUDE_PATHS)

    if config == 'debug':
        # /Z7 keeps the debug info inside the object files, so parallel
        # compiles cannot fight over one shared pdb file.
        env.Append(CCFLAGS=['/MTd', '/Z7', '/Od'], LINKFLAGS=['/DEBUG'])
    else:
        env.Append(CCFLAGS=['/MT', '/O2'], CPPDEFINES=['NDEBUG'])

    return env


def write_generated_files(build_root, code_dir='code'):
    """Write the small generated files the build needs.

    Returns the folder they end up in, as a real path.
    """
    generated = os.path.join(build_root, 'generated')
    os.makedirs(generated, exist_ok=True)

    # libpng ships a ready made config, it only needs a plain copy.
    shutil.copyfile(
        os.path.join(code_dir, 'third-party', 'libpng', 'scripts', 'pnglibconf.h.prebuilt'),
        os.path.join(generated, 'pnglibconf.h'))

    # OpenAL is configured by turning config.h.in into config.h.
    openal_dir = os.path.join(code_dir, 'third-party', 'openal')
    _write_openal_config(
        os.path.join(openal_dir, 'config.h.in'),
        os.path.join(generated, 'config.h'))
    _write_openal_version(os.path.join(generated, 'version.h'))
    _write_openal_hrtf_data(
        os.path.join(openal_dir, 'hrtf', 'Default HRTF.mhr'),
        os.path.join(generated, 'default_hrtf.txt'))

    # The editor DLL exports the compiler callback through this def file.
    with open(os.path.join(code_dir, 'tools', 'compilers', 'exports.def.in')) as f:
        template = f.read()
    text = template.replace('@DOOM_COMPILER_CALLBACK_SYMBOL@', '_DoomTools_RunW@16')
    with open(os.path.join(generated, 'compiler-exports.def'), 'w') as f:
        f.write(text)

    return generated


def _write_openal_config(in_path, out_path):
    """Turn config.h.in into config.h using the settings of this build."""
    enabled = set(OPENAL_CONFIG_DEFINES)
    with open(in_path) as f:
        lines = f.readlines()

    out = []
    for line in lines:
        match = re.match(r'#cmakedefine (\w+)', line)
        if match:
            name = match.group(1)
            if name in enabled:
                out.append('#define %s 1\n' % name)
            else:
                out.append('/* #undef %s */\n' % name)
        else:
            # The only other thing in the file is @NAME@ values that this
            # build does not use, so they are removed.
            out.append(re.sub(r'@\w+@', '', line))

    with open(out_path, 'w') as f:
        f.writelines(out)


def _write_openal_version(out_path):
    """The version header for the pinned OpenAL 1.23.1 sources."""
    text = (
        '/* Define to the library version */\n'
        '#define ALSOFT_VERSION "1.23.1"\n'
        '#define ALSOFT_VERSION_NUM 1,23,1,0\n'
        '\n'
        '/* Define the branch being built */\n'
        '#define ALSOFT_GIT_BRANCH "UNKNOWN"\n'
        '\n'
        '/* Define the hash of the head commit */\n'
        '#define ALSOFT_GIT_COMMIT_HASH "unknown"\n')
    with open(out_path, 'w') as f:
        f.write(text)


def _write_openal_hrtf_data(in_path, out_path):
    """Turn the HRTF data file into the hex list core/hrtf.cpp includes."""
    with open(in_path, 'rb') as f:
        data = f.read()
    with open(out_path, 'w') as f:
        for byte in data:
            f.write('    0x%02x,\n' % byte)


def run_tests(target, source, env):
    """Run every test program in source, called by "scons test"."""
    import subprocess

    failed = []
    for test in source:
        name = os.path.basename(test.path)
        print('Running %s' % name)
        code = subprocess.call([test.abspath])
        if code != 0:
            failed.append(name)

    if failed:
        return 'Failed tests: ' + ', '.join(failed)

    with open(target[0].abspath, 'w') as f:
        f.write('All tests passed.\n')
    return None
