# Source lists for every part of the project.
#
# The lists were moved over from the old CMake files. CMake wrote its paths
# relative to the code/ folder and used folder names that do not match the
# tree any more, so fix_path() below moves them to the folders that really
# exist today.

# The old CMake name on the left, the real folder on the right.
_FOLDER_MOVES = {
    'game/': 'game/game/',
    'cgame/': 'game/cgame/',
    'ui/': 'game/ui/',
    'botlib/': 'game/botlib/',
    'win32/': 'sys/win32/',
}

def fix_path(path):
    """Move one path from the old CMake layout to the real layout."""
    for old, new in _FOLDER_MOVES.items():
        if path.startswith(old):
            return new + path[len(old):]
    return path


def fix_paths(paths):
    """Move a whole list of paths to the real layout."""
    return [fix_path(path) for path in paths]


# The engine executable (CMake target "wolf", output Doom.exe).
WOLF_SOURCES = fix_paths([
    'client/cl_cgame.cpp',
    'client/cl_cin.cpp',
    'client/cl_console.cpp',
    'client/cl_input.cpp',
    'client/cl_keys.cpp',
    'client/cl_main.cpp',
    'client/cl_net_chan.cpp',
    'client/cl_parse.cpp',
    'client/cl_scrn.cpp',
    'client/cl_ui.cpp',
    'qcommon/cm_load.cpp',
    'qcommon/cm_patch.cpp',
    'qcommon/cm_polylib.cpp',
    'qcommon/cm_test.cpp',
    'qcommon/cm_trace.cpp',
    'qcommon/cmd.cpp',
    'qcommon/common.cpp',
    'qcommon/cvar.cpp',
    'qcommon/files.cpp',
    'qcommon/huffman.cpp',
    'qcommon/md4.cpp',
    'qcommon/msg.cpp',
    'qcommon/net_chan.cpp',
    'game/q_math.cpp',
    'game/q_shared.cpp',
    'server/sv_bot.cpp',
    'server/sv_ccmds.cpp',
    'server/sv_client.cpp',
    'server/sv_game.cpp',
    'server/sv_init.cpp',
    'server/sv_main.cpp',
    'server/sv_net_chan.cpp',
    'server/sv_snapshot.cpp',
    'server/sv_world.cpp',
    'qcommon/unzip.cpp',
    'sys/sys_input.cpp',
    'sys/sys_events.cpp',
    'sys/sys_main.cpp',
    'sys/win32/win_net.cpp',
    'sys/sys_shared.cpp',
    'sys/win32/win_shared.cpp',
    'sys/win32/win_syscon.cpp',
])

# Extra engine files that CMake added on top of the main list. The resource
# file winquake.rc is built separately in the SConstruct file, because SCons
# does not compile .rc files that sit in a Program source list.
WOLF_EXTRA_SOURCES = fix_paths([
    'client/cl_console_font.cpp',
    'qcommon/doom_content.cpp',
    'sys/sys_editor.cpp',
    'sound/snd_openal.cpp',
    'sound/snd_music.cpp',
    'sound/snd_fluidsynth.cpp',
    'sound/snd_musicmix.cpp',
    'sound/snd_wave.cpp',
    'sound/snd_stream.cpp',
])

BOTLIB_SOURCES = fix_paths([
    'botlib/be_aas_bspq3.cpp',
    'botlib/be_aas_cluster.cpp',
    'botlib/be_aas_debug.cpp',
    'botlib/be_aas_entity.cpp',
    'botlib/be_aas_file.cpp',
    'botlib/be_aas_main.cpp',
    'botlib/be_aas_move.cpp',
    'botlib/be_aas_optimize.cpp',
    'botlib/be_aas_reach.cpp',
    'botlib/be_aas_route.cpp',
    'botlib/be_aas_routealt.cpp',
    'botlib/be_aas_routetable.cpp',
    'botlib/be_aas_sample.cpp',
    'botlib/be_ai_char.cpp',
    'botlib/be_ai_chat.cpp',
    'botlib/be_ai_gen.cpp',
    'botlib/be_ai_goal.cpp',
    'botlib/be_ai_move.cpp',
    'botlib/be_ai_weap.cpp',
    'botlib/be_ai_weight.cpp',
    'botlib/be_ea.cpp',
    'botlib/be_interface.cpp',
    'botlib/l_crc.cpp',
    'botlib/l_libvar.cpp',
    'botlib/l_log.cpp',
    'botlib/l_memory.cpp',
    'botlib/l_precomp.cpp',
    'botlib/l_script.cpp',
    'botlib/l_struct.cpp',
])

# The client game module (cgamex86.dll). game/bg_lib.cpp stays out, it is a
# libc for the old bytecode VM and is not used by the native build.
CGAME_SOURCES = fix_paths([
    'game/bg_doommonsters.cpp',
    'game/bg_animation.cpp',
    'game/bg_misc.cpp',
    'game/bg_pmove.cpp',
    'game/bg_slidemove.cpp',
    'cgame/cg_consolecmds.cpp',
    'cgame/cg_draw.cpp',
    'cgame/cg_drawtools.cpp',
    'cgame/cg_effects.cpp',
    'cgame/cg_ents.cpp',
    'cgame/cg_event.cpp',
    'cgame/cg_flamethrower.cpp',
    'cgame/cg_info.cpp',
    'cgame/cg_localents.cpp',
    'cgame/cg_main.cpp',
    'cgame/cg_marks.cpp',
    'cgame/cg_newDraw.cpp',
    'cgame/cg_particles.cpp',
    'cgame/cg_players.cpp',
    'cgame/cg_playerstate.cpp',
    'cgame/cg_predict.cpp',
    'cgame/cg_scoreboard.cpp',
    'cgame/cg_servercmds.cpp',
    'cgame/cg_snapshot.cpp',
    'cgame/cg_sound.cpp',
    'cgame/cg_import.cpp',
    'cgame/cg_trails.cpp',
    'cgame/cg_view.cpp',
    'cgame/cg_weapons.cpp',
    'game/q_math.cpp',
    'game/q_shared.cpp',
    'ui/ui_shared.cpp',
])

CGAME_EXTRA_SOURCES = fix_paths([
    'cgame/cg_doomhud.cpp',
    'cgame/cg_doomweapons.cpp',
    'cgame/cg_weaponanim.cpp',
    'cgame/cg_doomactors.cpp',
    'cgame/cg_doomitems.cpp',
    'cgame/cg_doommap.cpp',
    'cgame/cg_doomscreens.cpp',
    'cgame/cg_doommedia.cpp',
    'cgame/cg_damageview.cpp',
    'cgame/cg_doomfx.cpp',
    'cgame/cg_doomfx_materials.cpp',
    'cgame/cg_doomfx_blood.cpp',
    'cgame/cgame.def',
])

# The Doom 3 style effect system, compiled into the client game module.
DOOM_FX_SOURCES = fix_paths([
    'cgame/fx/genericparser2.cpp',
    'cgame/fx/FxTemplate.cpp',
    'cgame/fx/FxFlame.cpp',
    'cgame/fx/FxScheduler.cpp',
    'cgame/fx/FxPrimitives.cpp',
    'cgame/fx/FxUtil.cpp',
    'cgame/fx/FxSystem.cpp',
])

# The server game module (qagamex86.dll). bg_lib.cpp stays out, see above.
GAME_SOURCES = fix_paths([
    'game/g_doomai.cpp',
    'game/g_doomai_services.cpp',
    'game/bg_doommonsters.cpp',
    'game/bg_animation.cpp',
    'game/bg_misc.cpp',
    'game/bg_pmove.cpp',
    'game/bg_slidemove.cpp',
    'game/g_active.cpp',
    'game/g_alarm.cpp',
    'game/g_client.cpp',
    'game/g_cmds.cpp',
    'game/g_combat.cpp',
    'game/g_items.cpp',
    'game/g_main.cpp',
    'game/g_mem.cpp',
    'game/g_misc.cpp',
    'game/g_missile.cpp',
    'game/g_mover.cpp',
    'game/g_props.cpp',
    'game/g_save.cpp',
    'game/g_script.cpp',
    'game/g_script_actions.cpp',
    'game/g_session.cpp',
    'game/g_spawn.cpp',
    'game/g_svcmds.cpp',
    'game/g_import.cpp',
    'game/g_target.cpp',
    'game/g_team.cpp',
    'game/g_tramcar.cpp',
    'game/g_trigger.cpp',
    'game/g_utils.cpp',
    'game/g_weapon.cpp',
    'game/q_math.cpp',
    'game/q_shared.cpp',
])

GAME_EXTRA_SOURCES = fix_paths([
    'game/g_doom.cpp',
    'game/g_doomweapons.cpp',
    'game/g_doomitems.cpp',
    'game/g_doomlevel.cpp',
    'game/game.def',
    'game/g_save_callbacks.cpp',
    'game/g_doomsave.cpp',
    'game/g_doombots.cpp',
    'game/g_doombots_navigation.cpp',
    'game/g_doombots_combat.cpp',
    'game/g_doombots_goals.cpp',
])

# The menu module (uix86.dll). bg_lib.cpp stays out, see above.
UI_SOURCES = fix_paths([
    'game/bg_misc.cpp',
    'game/q_math.cpp',
    'game/q_shared.cpp',
    'ui/ui_atoms.cpp',
    'ui/ui_gameinfo.cpp',
    'ui/ui_main.cpp',
    'ui/ui_players.cpp',
    'ui/ui_shared.cpp',
    'ui/ui_import.cpp',
    'ui/ui_util.cpp',
])

UI_EXTRA_SOURCES = fix_paths([
    'ui/ui.def',
    'ui/ui_doom.cpp',
])

RENDERER_SOURCES = fix_paths([
    'renderer/tr_animation.cpp',
    'renderer/tr_backend.cpp',
    'renderer/tr_bsp.cpp',
    'renderer/tr_cmds.cpp',
    'renderer/tr_cmesh.cpp',
    'renderer/tr_curve.cpp',
    'renderer/tr_flares.cpp',
    'renderer/tr_font.cpp',
    'renderer/tr_image.cpp',
    'renderer/tr_init.cpp',
    'renderer/tr_light.cpp',
    'renderer/tr_main.cpp',
    'renderer/tr_marks.cpp',
    'renderer/tr_mesh.cpp',
    'renderer/tr_model.cpp',
    'renderer/tr_noise.cpp',
    'renderer/tr_scene.cpp',
    'renderer/tr_shade.cpp',
    'renderer/tr_shade_calc.cpp',
    'renderer/tr_shader.cpp',
    'renderer/tr_shadows.cpp',
    'renderer/tr_sky.cpp',
    'renderer/tr_surface.cpp',
    'renderer/tr_world.cpp',
    'sys/sys_glimp.cpp',
    'sys/sys_qgl.cpp',
])

RENDERER_EXTRA_SOURCES = fix_paths([
    'renderer/tr_doom.cpp',
    'renderer/tr_glprogs.cpp',
    'renderer/tr_vfx.cpp',
    'renderer/tr_decals.cpp',
    'renderer/tr_hbao.cpp',
    'renderer/tr_editor.cpp',
    'sys/sys_png.cpp',
])

SPLINES_SOURCES = fix_paths([
    'splines/math_angles.cpp',
    'splines/math_matrix.cpp',
    'splines/math_quaternion.cpp',
    'splines/math_vector.cpp',
    'splines/q_parse.cpp',
    'splines/q_shared.cpp',
    'splines/splines.cpp',
    'splines/util_str.cpp',
])

OPL_SOURCES = fix_paths([
    'sound/snd_opl.cpp',
    'sound/opl/doom_opl.cpp',
    'sound/opl/opl3.c',
])

# miniz replaces the old zlib: one C file, built as a small static library.
# Paths are code/ relative.
MINIZ_SOURCES = [
    'third-party/miniz/miniz.c',
]

DMAP_SOURCES = fix_paths([
    'tools/compilers/dmap/main.cpp',
    'tools/compilers/dmap/dwd.cpp',
    'tools/compilers/dmap/wad.cpp',
    'tools/compilers/dmap/bsp.cpp',
])

AASBUILD_SOURCES = fix_paths([
    'tools/compilers/aasbuild/doom_seal.cpp',
    'tools/compilers/aasbuild/_files.cpp',
    'tools/compilers/aasbuild/aas_areamerging.cpp',
    'tools/compilers/aasbuild/aas_cfg.cpp',
    'tools/compilers/aasbuild/aas_create.cpp',
    'tools/compilers/aasbuild/aas_edgemelting.cpp',
    'tools/compilers/aasbuild/aas_facemerging.cpp',
    'tools/compilers/aasbuild/aas_file.cpp',
    'tools/compilers/aasbuild/aas_gsubdiv.cpp',
    'tools/compilers/aasbuild/aas_map.cpp',
    'tools/compilers/aasbuild/aas_prunenodes.cpp',
    'tools/compilers/aasbuild/aas_store.cpp',
    'tools/compilers/aasbuild/be_aas_bspc.cpp',
    'botlib/be_aas_bspq3.cpp',
    'botlib/be_aas_cluster.cpp',
    'botlib/be_aas_move.cpp',
    'botlib/be_aas_optimize.cpp',
    'botlib/be_aas_reach.cpp',
    'botlib/be_aas_sample.cpp',
    'tools/compilers/aasbuild/brushbsp.cpp',
    'tools/compilers/aasbuild/legacy_driver.cpp',
    'tools/compilers/aasbuild/main.cpp',
    'qcommon/cm_load.cpp',
    'qcommon/cm_patch.cpp',
    'qcommon/cm_test.cpp',
    'qcommon/cm_trace.cpp',
    'tools/compilers/aasbuild/csg.cpp',
    'tools/compilers/aasbuild/glfile.cpp',
    'tools/compilers/aasbuild/l_bsp_ent.cpp',
    'tools/compilers/aasbuild/l_bsp_hl.cpp',
    'tools/compilers/aasbuild/l_bsp_q1.cpp',
    'tools/compilers/aasbuild/l_bsp_q2.cpp',
    'tools/compilers/aasbuild/l_bsp_q3.cpp',
    'tools/compilers/aasbuild/l_bsp_sin.cpp',
    'tools/compilers/aasbuild/l_cmd.cpp',
    'botlib/l_libvar.cpp',
    'tools/compilers/aasbuild/l_log.cpp',
    'tools/compilers/aasbuild/l_math.cpp',
    'tools/compilers/aasbuild/l_mem.cpp',
    'tools/compilers/aasbuild/l_poly.cpp',
    'botlib/l_precomp.cpp',
    'tools/compilers/aasbuild/l_qfiles.cpp',
    'botlib/l_script.cpp',
    'botlib/l_struct.cpp',
    'tools/compilers/aasbuild/l_threads.cpp',
    'tools/compilers/aasbuild/l_utils.cpp',
    'tools/compilers/aasbuild/leakfile.cpp',
    'tools/compilers/aasbuild/map.cpp',
    'tools/compilers/aasbuild/map_hl.cpp',
    'tools/compilers/aasbuild/map_q1.cpp',
    'tools/compilers/aasbuild/map_q2.cpp',
    'tools/compilers/aasbuild/map_q3.cpp',
    'tools/compilers/aasbuild/map_sin.cpp',
    'qcommon/md4.cpp',
    'tools/compilers/aasbuild/nodraw.cpp',
    'tools/compilers/aasbuild/portals.cpp',
    'tools/compilers/aasbuild/textures.cpp',
    'tools/compilers/aasbuild/tree.cpp',
    'qcommon/unzip.cpp',
    'tools/compilers/aasbuild/cpp_bridge.cpp',
])

# AAS navigation profiles, copied next to the game data after the editor is
# built.
AAS_CONFIG_FILES = [
    'tools/compilers/aasbuild/doom_small.cfg',
    'tools/compilers/aasbuild/doom_medium.cfg',
    'tools/compilers/aasbuild/doom_cyber.cfg',
    'tools/compilers/aasbuild/doom_spider.cfg',
]

# The editor core, the part the editor DLL and the editor tests share.
EDITOR_CORE_SOURCES = [
    'tools/editor/EditWorld.cpp',
    'tools/editor/ThingSpline.cpp',
    'tools/editor/EWLoadSave.cpp',
    'tools/editor/EWDoomSave.cpp',
    'tools/editor/BlockWorld.cpp',
    'tools/editor/Wadfile.cpp',
    'tools/editor/MaterialPack.cpp',
    'tools/editor/DoomProject.cpp',
    'tools/editor/MapTextures.cpp',
    'tools/editor/idfunctions.cpp',
    'tools/editor/pathops.cpp',
    'tools/editor/lbmfunctions.cpp',
    'tools/editor/SpecialList.cpp',
    'tools/editor/Timing.cpp',
    'tools/editor/SectorAuthoring.cpp',
]

# The rest of the editor is every .cpp in tools/editor that is not in the core
# list above and not one of these. Some of these names no longer exist, they
# are kept here so the list matches the old CMake build.
EDITOR_EXCLUDED_SOURCES = set(EDITOR_CORE_SOURCES) | {
    'tools/editor/SpriteModel.cpp',
    'tools/editor/SpriteRenderer.cpp',
    'tools/editor/DoomEd_main.cpp',
    'tools/editor/IntegratedEditor.cpp',
    'tools/editor/SectorAuthoring.cpp',
    'tools/editor/NextUI.cpp',
    'tools/editor/NextAssets.cpp',
    'tools/editor/SpriteTool.cpp',
}

EDITOR_EXTRA_SOURCES = [
    'tools/editor/IntegratedEditor.cpp',
    'tools/editor/WadCache/WadCache.cpp',
    'tools/compilers/CompilerJob.cpp',
    'tools/compilers/CompilerEntry.cpp',
]

IMGUI_SOURCES = [
    'third-party/imgui/imgui.cpp',
    'third-party/imgui/imgui_draw.cpp',
    'third-party/imgui/imgui_tables.cpp',
    'third-party/imgui/imgui_widgets.cpp',
    'third-party/imgui/backends/imgui_impl_sdl3.cpp',
]



# The test programs. Each entry is a dictionary:
#   name     - the exe name (built as bin/tests/<name>)
#   sources  - the files to compile; test files live in test/, engine files under code/
#   args     - the command line the test runner passes (optional)
#   defines  - extra defines (optional)
#   libs     - static libraries from this build to link in (optional)
#   syslibs  - system libraries to link in (optional)
#   sections - True when the old build used the smaller sections flags
TESTS = [
    {
        'name': 'doommap_format_tests',
        'sources': ['test/dmap/format_tests.cpp'],
        'args': ['base/maps/e1m1.bsp'],
    },
    {
        'name': 'doom_menu_tests',
        'sources': ['test/ui/doom_menu_tests.cpp'],
    },
    {
        'name': 'doom_content_tests',
        'sources': ['test/qcommon/doom_content_tests.cpp', 'qcommon/doom_content.cpp'],
    },
    {
        'name': 'doom_damage_view_tests',
        'sources': ['test/cgame/doom_damage_view_tests.cpp'],
        'sections': True,
    },
    {
        'name': 'doom_display_tests',
        'sources': ['test/ui/display_tests.cpp'],
    },
    {
        'name': 'doom_hud_tests',
        'sources': ['test/cgame/doom_hud_tests.cpp'],
    },
    {
        'name': 'doom_weapon_tests',
        'sources': ['test/game/doom_weapon_tests.cpp'],
    },
    {
        'name': 'doom_png_tests',
        'sources': ['test/renderer/png_tests.cpp', 'sys/sys_png.cpp'],
        # win_png.cpp carries the stb_image implementation and the fixtures
        # are hand rolled, so the test needs no extra libraries.
    },
    {
        'name': 'doom_sprite_animation_tests',
        'sources': [
            'test/cgame/doom_sprite_animation_tests.cpp',
            'cgame/cg_weaponanim.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_weapon_server_tests',
        'sources': [
            'test/game/doom_weapon_server_tests.cpp',
            'game/g_doomweapons.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_monster_tests',
        'sources': [
            'test/game/doom_monster_tests.cpp',
            'game/bg_doommonsters.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_monster_server_tests',
        'sources': [
            'test/game/doom_monster_server_tests.cpp',
            'game/g_doomai.cpp',
            'game/bg_doommonsters.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
            'game/bg_pmove.cpp',
            'game/bg_slidemove.cpp',
            'game/bg_animation.cpp',
            'game/bg_misc.cpp',
        ],
        'defines': ['GAMEDLL'],
        'sections': True,
    },
    {
        'name': 'doom_shader_index_tests',
        'sources': [
            'test/renderer/shader_index_tests.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_replication_tests',
        'sources': [
            'test/game/doom_replication_tests.cpp',
            'qcommon/msg.cpp',
            'qcommon/huffman.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_sprite_pmove_tests',
        'sources': [
            'test/game/doom_sprite_pmove_tests.cpp',
            'game/bg_pmove.cpp',
            'game/bg_slidemove.cpp',
            'game/bg_animation.cpp',
            'game/bg_misc.cpp',
            'game/bg_doommonsters.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'defines': ['GAMEDLL'],
        'sections': True,
    },
    {
        'name': 'doom_item_tests',
        'sources': [
            'test/game/doom_item_tests.cpp',
            'game/g_doomitems.cpp',
            'game/bg_doommonsters.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_teleport_tests',
        'sources': ['test/game/doom_teleport_tests.cpp', 'game/q_shared.cpp', 'game/q_math.cpp'],
        'sections': True,
    },
    {
        'name': 'doom_light_tests',
        'sources': ['test/renderer/doom_light_tests.cpp'],
    },
    {
        'name': 'doom_level_tests',
        'sources': [
            'test/game/doom_level_tests.cpp',
            'game/g_doomlevel.cpp',
            'game/bg_doommonsters.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_screen_tests',
        'sources': [
            'test/cgame/doom_screen_tests.cpp',
            'cgame/cg_doomscreens.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_audio_tests',
        'sources': [
            'test/sound/audio_tests.cpp',
            'sound/snd_fluidsynth.cpp',
            'sound/snd_wave.cpp',
        ],
        'libs': ['OpenAL'],
        'syslibs': ['winmm', 'ole32', 'oleaut32'],
    },
    {
        'name': 'doom_music_stream_tests',
        'sources': [
            'test/sound/music_stream_tests.cpp',
            'sound/snd_music.cpp',
            'sound/snd_fluidsynth.cpp',
            'sound/snd_musicmix.cpp',
            'sound/snd_stream.cpp',
            'sound/snd_wave.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'libs': ['OpenAL', 'doom_opl'],
        'syslibs': ['winmm', 'ole32', 'oleaut32'],
        'sections': True,
    },
    {
        'name': 'doom_opl_tests',
        'sources': ['test/sound/opl_tests.cpp', 'sound/snd_musicmix.cpp'],
        'libs': ['doom_opl'],
    },
    {
        'name': 'doom_fx_tests',
        'sources': [
            'test/cgame/doom_fx_tests.cpp',
            'cgame/cg_marks.cpp',
        ] + [
            'cgame/fx/genericparser2.cpp',
            'cgame/fx/FxTemplate.cpp',
            'cgame/fx/FxFlame.cpp',
            'cgame/fx/FxScheduler.cpp',
            'cgame/fx/FxPrimitives.cpp',
            'cgame/fx/FxUtil.cpp',
            'cgame/fx/FxSystem.cpp',
        ] + [
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_fx_material_tests',
        'sources': ['test/cgame/doom_fx_material_tests.cpp', 'cgame/cg_doomfx_materials.cpp'],
    },
    {
        'name': 'doom_blood_tests',
        'sources': [
            'test/cgame/doom_blood_tests.cpp',
            'cgame/cg_doomfx_blood.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_netchan_tests',
        'sources': [
            'test/game/doom_netchan_tests.cpp',
            'qcommon/net_chan.cpp',
            'qcommon/msg.cpp',
            'qcommon/huffman.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_save_callbacks_tests',
        'sources': [
            'test/game/doom_save_callbacks_tests.cpp',
            'test/game/doom_save_callbacks_other.cpp',
            'game/g_save_callbacks.cpp',
        ],
        'defines': ['DOOM_SAVE_CALLBACK_TEST'],
    },
    {
        'name': 'doom_save_header_tests',
        'sources': ['test/game/doom_save_header_tests.cpp'],
    },
    {
        'name': 'doom_save_clients_tests',
        'sources': [
            'test/game/doom_save_clients_tests.cpp',
            'game/g_doomsave.cpp',
            'game/bg_doommonsters.cpp',
            'game/q_shared.cpp',
            'game/q_math.cpp',
        ],
        'sections': True,
    },
    {
        'name': 'doom_bot_tests',
        'sources': ['test/game/doom_bot_tests.cpp', 'game/q_shared.cpp', 'game/q_math.cpp'],
        'sections': True,
    },
    {
        'name': 'doomed_authoring_tests',
        'sources': ['test/editor/AuthoringTests.cpp'],
        'libs': ['doomed_core'],
        'syslibs': ['windowscodecs', 'ole32'],
    },
    {
        'name': 'doomed_host_tests',
        'sources': ['test/editor/HostTests.cpp'],
        'args': ['bin/toolsx86.dll', 'build/test-scratch/doomed_host',
                 'base/wad/DOOM.WAD', '.'],
        'libs': ['doomed_core'],
        'syslibs': ['windowscodecs', 'ole32', 'user32', 'psapi'],
    },
    {
        'name': 'doomed_port_tests',
        'sources': ['test/editor/PortTests.cpp'],
        'args': ['.', 'build/test-scratch/doomed_port'],
        'libs': ['doomed_core'],
        'syslibs': ['windowscodecs', 'ole32'],
    },
    {
        'name': 'doomed_core_tests',
        'sources': ['test/editor/CoreTests.cpp'],
        'args': ['base/wad/DOOM.WAD', 'build/test-scratch/doomed_core'],
        'libs': ['doomed_core'],
        'syslibs': ['windowscodecs', 'ole32'],
    },
]



