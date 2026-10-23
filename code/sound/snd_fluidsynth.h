// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SND_FLUIDSYNTH_H
#define SND_FLUIDSYNTH_H

#include "fluidsynth.h"

namespace doomaudio {

// FluidSynth entry points resolved at runtime from libfluidsynth-3.dll, the
// same idea as the optional OpenAL extension entry points. The build links
// no import library, so the DLL stays optional and the engine still boots
// (and plays OPL2 and PCM music) when it is missing.
struct FluidSynth {
    void *module = nullptr;

    fluid_settings_t *(*new_settings)() = nullptr;
    void (*delete_settings)(fluid_settings_t *) = nullptr;
    int (*settings_setstr)(fluid_settings_t *, const char *, const char *) = nullptr;
    int (*settings_setnum)(fluid_settings_t *, const char *, double) = nullptr;
    fluid_synth_t *(*new_synth)(fluid_settings_t *) = nullptr;
    void (*delete_synth)(fluid_synth_t *) = nullptr;
    int (*sfload)(fluid_synth_t *, const char *filename, int reset_presets) = nullptr;
    int (*write_s16)(fluid_synth_t *, int frames, void *left, int leftOffset, int leftStep,
                     void *right, int rightOffset, int rightStep) = nullptr;
    fluid_player_t *(*new_player)(fluid_synth_t *) = nullptr;
    void (*delete_player)(fluid_player_t *) = nullptr;
    int (*player_add_mem)(fluid_player_t *, const void *buffer, size_t len) = nullptr;
    int (*player_play)(fluid_player_t *) = nullptr;
    int (*player_seek)(fluid_player_t *, int ticks) = nullptr;
    int (*player_get_status)(fluid_player_t *) = nullptr;
    int (*player_get_current_tick)(fluid_player_t *) = nullptr;
    int (*player_get_total_ticks)(fluid_player_t *) = nullptr;

    bool Load();   // load the DLL and resolve every entry point
    void Unload(); // free the DLL; the caller destroys its objects first
};

// The one shared loader instance, used by the engine and the audio tests.
extern FluidSynth fluidsynth;

} // namespace doomaudio

#endif
