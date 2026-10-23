// SPDX-License-Identifier: GPL-3.0-or-later
#include "snd_fluidsynth.h"
#include <windows.h>

namespace doomaudio {

bool FluidSynth::Load() {
    if (module) {
        return true;
    }
    module = LoadLibrary("libfluidsynth-3.dll");
    if (!module) {
        return false;
    }
    struct Entry {
        const char *name;
        void **target;
    };
    const Entry entries[] = {
        {"new_fluid_settings", (void **)&new_settings},
        {"delete_fluid_settings", (void **)&delete_settings},
        {"fluid_settings_setstr", (void **)&settings_setstr},
        {"fluid_settings_setnum", (void **)&settings_setnum},
        {"new_fluid_synth", (void **)&new_synth},
        {"delete_fluid_synth", (void **)&delete_synth},
        {"fluid_synth_sfload", (void **)&sfload},
        {"fluid_synth_write_s16", (void **)&write_s16},
        {"new_fluid_player", (void **)&new_player},
        {"delete_fluid_player", (void **)&delete_player},
        {"fluid_player_add_mem", (void **)&player_add_mem},
        {"fluid_player_play", (void **)&player_play},
        {"fluid_player_seek", (void **)&player_seek},
        {"fluid_player_get_status", (void **)&player_get_status},
        {"fluid_player_get_current_tick", (void **)&player_get_current_tick},
        {"fluid_player_get_total_ticks", (void **)&player_get_total_ticks},
    };
    for (const Entry &entry : entries) {
        *entry.target = reinterpret_cast<void *>(GetProcAddress((HMODULE)module, entry.name));
        if (!*entry.target) {
            Unload();
            return false;
        }
    }
    return true;
}

void FluidSynth::Unload() {
    if (module) {
        FreeLibrary((HMODULE)module);
        module = nullptr;
    }
    new_settings = nullptr;
    delete_settings = nullptr;
    settings_setstr = nullptr;
    settings_setnum = nullptr;
    new_synth = nullptr;
    delete_synth = nullptr;
    sfload = nullptr;
    write_s16 = nullptr;
    new_player = nullptr;
    delete_player = nullptr;
    player_add_mem = nullptr;
    player_play = nullptr;
    player_seek = nullptr;
    player_get_status = nullptr;
    player_get_current_tick = nullptr;
    player_get_total_ticks = nullptr;
}

FluidSynth fluidsynth;

} // namespace doomaudio
