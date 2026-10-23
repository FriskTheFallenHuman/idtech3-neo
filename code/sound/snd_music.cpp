// SPDX-License-Identifier: GPL-3.0-or-later
#include "../client/client.h"
#include "snd_local.h"
#include "snd_opl.h"
#include "snd_musicmix.h"
#include "snd_fluidsynth.h"
#include "../openal/include/AL/al.h"
#include <vector>
namespace doomaudio {
namespace {
fluid_settings_t *settings;
fluid_synth_t *synth;
fluid_player_t *player;
ALuint source;
bool synthesizer;
char introName[MAX_QPATH], loopName[MAX_QPATH];
std::vector<byte> midi;
Wave wave;
size_t wavePosition;
int loops, chunks;
MusicMix musicMix;
bool Load(const char *name) {
    OPLStop();
    if (player) {
        fluidsynth.delete_player(player);
        player = nullptr;
    }
    wave = {};
    wavePosition = 0;
    midi.clear();
    if (!name || !*name) {
        return false;
    }
    void *bytes = nullptr;
    const int length = FS_ReadFile(name, &bytes);
    if (length <= 0 || length > 32 * 1024 * 1024) {
        if (bytes) {
            FS_FreeFile(bytes);
        }
        Com_Printf("Music: cannot read %s\n", name);
        return false;
    }
    const auto *data = static_cast<const byte *>(bytes);
    if (length >= 4 && !memcmp(data, "MThd", 4) &&
        Q_stricmp(Cvar_VariableString("s_musicBackend"), "fluidsynth")) {
        void *instruments = nullptr;
        const int instrumentLength = FS_ReadFile("sound/doom/genmidi.op2", &instruments);
        const bool valid =
            instrumentLength > 0 && instrumentLength <= 16384 &&
            OPLLoad(data, length, static_cast<const byte *>(instruments), instrumentLength);
        if (instruments) {
            FS_FreeFile(instruments);
        }
        FS_FreeFile(bytes);
        if (!valid) {
            Com_Printf("Doom OPL: cannot decode %s: %s\n", name, OPLError());
            return false;
        }
        Com_Printf("Doom OPL2 music: %s (%u ms, original GENMIDI)\n", name, OPLDuration());
        return true;
    }
    if (length >= 4 && !memcmp(data, "MThd", 4) && synthesizer) {
        midi.assign(data, data + length);
        FS_FreeFile(bytes);
        player = fluidsynth.new_player(synth);
        if (!player || fluidsynth.player_add_mem(player, midi.data(), midi.size()) != FLUID_OK) {
            if (player) {
                fluidsynth.delete_player(player);
                player = nullptr;
            }
            Com_Printf("FluidSynth: cannot decode %s\n", name);
            return false;
        }
        fluidsynth.player_play(player);
        Com_Printf("FluidSynth music: %s (%d ticks)\n", name,
                   fluidsynth.player_get_total_ticks(player));
        return true;
    }
    const bool decoded = DecodeWave(data, length, wave);
    FS_FreeFile(bytes);
    if (!decoded) {
        Com_Printf("Music: unsupported or malformed track %s\n", name);
    }
    return decoded;
}
} // namespace
void MusicInit() {
    Cvar_Get("s_musicBackend", "opl", CVAR_ARCHIVE);
    Cvar_Get("s_musicGain", "6", CVAR_ARCHIVE);
    Cvar_Get("s_musicBass", "4", CVAR_ARCHIVE);
    source = NewStreamSource();
    if (fluidsynth.Load()) {
        settings = fluidsynth.new_settings();
        if (settings) {
            fluidsynth.settings_setnum(settings, "synth.sample-rate", 44100.0);
            // The sample clock drives the MIDI player, so rendering pulls
            // the timeline along with the audio instead of wall time.
            fluidsynth.settings_setstr(settings, "player.timing-source", "sample");
        }
        synth = settings ? fluidsynth.new_synth(settings) : nullptr;
    }
    if (synth) {
        char path[MAX_OSPATH];
        Com_sprintf(path, sizeof(path), "%s/base/sound/timidity/TimGM6mb.sf2",
                    Cvar_VariableString("fs_basepath"));
        if (fluidsynth.sfload(synth, path, 1) == FLUID_FAILED) {
            Com_Printf("FluidSynth: cannot read SoundFont %s\n", path);
        } else {
            synthesizer = true;
        }
    }
    if (!synthesizer) {
        if (synth) {
            fluidsynth.delete_synth(synth);
            synth = nullptr;
        }
        if (settings) {
            fluidsynth.delete_settings(settings);
            settings = nullptr;
        }
        fluidsynth.Unload();
    }
}
void MusicStop() {
    OPLStop();
    musicMix.Reset();
    ClearStream(source);
    if (player) {
        fluidsynth.delete_player(player);
        player = nullptr;
    }
    wave = {};
    midi.clear();
    wavePosition = 0;
    loopName[0] = 0;
    introName[0] = 0;
}
void MusicShutdown() {
    MusicStop();
    if (source) {
        alDeleteSources(1, &source);
    }
    source = 0;
    if (synth) {
        fluidsynth.delete_synth(synth);
        synth = nullptr;
    }
    if (settings) {
        fluidsynth.delete_settings(settings);
        settings = nullptr;
    }
    fluidsynth.Unload();
    synthesizer = false;
}
void MusicStart(const char *intro, const char *loop) {
    const char *repeat = loop && *loop ? loop : intro;
    const bool wantsOPL = Q_stricmp(Cvar_VariableString("s_musicBackend"), "fluidsynth") != 0;
    const bool sameBackend = wantsOPL ? OPLLoaded() : player != nullptr;
    if (intro && repeat && (sameBackend || !wave.samples.empty()) && !Q_stricmp(introName, intro) &&
        !Q_stricmp(loopName, repeat)) {
        return;
    }
    MusicStop();
    loops = chunks = 0;
    if (!intro || !*intro) {
        return;
    }
    Q_strncpyz(loopName, loop && *loop ? loop : intro, sizeof(loopName));
    Q_strncpyz(introName, intro, sizeof(introName));
    Load(intro);
}
void MusicInfo() {
    ALint queued = 0, state = AL_STOPPED;
    if (source) {
        alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
        alGetSourcei(source, AL_SOURCE_STATE, &state);
    }
    Com_Printf("Doom music: track=%s playing=%d queued=%d position=%u loops=%d backend=%s "
               "gain=%.1fdB bass=%.1fdB\n",
               introName, state == AL_PLAYING, queued,
               player ? (unsigned)fluidsynth.player_get_current_tick(player) : OPLPosition(), loops,
               OPLLoaded() ? "opl2"
               : player   ? "fluidsynth"
                          : "pcm",
               OPLLoaded() ? Cvar_VariableValue("s_musicGain") : 0,
               OPLLoaded() ? Cvar_VariableValue("s_musicBass") : 0);
}
void MusicUpdate(float volume) {
    if (!source) {
        return;
    }
    alSourcef(source, AL_GAIN, volume);
    ALint queued = 0, processed = 0;
    alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
    int emptyLoops = 0;
    for (int count = queued - processed;
         count < 4 && (player || OPLLoaded() || !wave.samples.empty()); ++count) {
        int16_t pcm[8192];
        size_t size = 0;
        int rate = 44100, channels = 2;
        if (OPLLoaded()) {
            size = OPLRender(pcm, sizeof(pcm) / (sizeof(pcm[0]) * 2)) * sizeof(pcm[0]) * 2;
        } else if (player) {
            if (fluidsynth.player_get_status(player) != FLUID_PLAYER_PLAYING) {
                size = 0;
            } else {
                // Render interleaved stereo straight into the buffer.
                fluidsynth.write_s16(synth, sizeof(pcm) / (sizeof(pcm[0]) * 2), pcm, 0, 2, pcm, 1,
                                     2);
                size = sizeof(pcm);
            }
        } else {
            rate = wave.rate;
            channels = wave.channels;
            const size_t samples = min(size_t(8192), wave.samples.size() - wavePosition);
            memcpy(pcm, wave.samples.data() + wavePosition, samples * 2);
            wavePosition += samples;
            size = samples * 2;
        }
        if (!size) {
            if (!loopName[0]) {
                break;
            }
            char next[MAX_QPATH];
            Q_strncpyz(next, loopName, sizeof(next));
            if (!Load(next)) {
                loopName[0] = 0;
                break;
            }
            ++loops;
            if (++emptyLoops > 1) {
                Com_Printf("Music: empty loop rejected: %s\n", next);
                MusicStop();
                break;
            }
            --count;
            continue;
        }
        musicMix.Configure(rate, OPLLoaded() ? Cvar_VariableValue("s_musicGain") : 0,
                           OPLLoaded() ? Cvar_VariableValue("s_musicBass") : 0);
        musicMix.Process(pcm, size / (sizeof(pcm[0]) * channels), channels);
        QueuePCM(source, pcm, static_cast<int>(size), rate, channels, 2);
        ++chunks;
    }
    if ((player || OPLLoaded() || !wave.samples.empty()) && queued > 0) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            alSourcePlay(source);
        }
    }
}
} // namespace doomaudio
