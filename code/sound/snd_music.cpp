// SPDX-License-Identifier: GPL-3.0-or-later
#include "../client/client.h"
#include "snd_local.h"
#include "snd_opl.h"
#include "snd_musicmix.h"
#include "../openal/include/AL/al.h"
#include "../timidy/src/timidity.h"
#include <vector>
namespace doomaudio {
namespace {
MidSong *song;
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
    if (song) {
        mid_song_free(song);
        song = nullptr;
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
        Q_stricmp(Cvar_VariableString("s_musicBackend"), "timidity")) {
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
        auto *stream = mid_istream_open_mem(midi.data(), midi.size());
        MidSongOptions options{44100, MID_AUDIO_S16LSB, 2, 0, 4096, 0};
        if (stream) {
            song = mid_song_load(stream, &options);
            mid_istream_close(stream);
        }
        if (!song) {
            Com_Printf("TiMidity: cannot decode %s\n", name);
            return false;
        }
        mid_song_start(song);
        Com_Printf("TiMidity music: %s (%u ms)\n", name, mid_song_get_total_time(song));
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
    char path[MAX_OSPATH];
    Com_sprintf(path, sizeof(path), "%s/base/sound/timidity/timidity.cfg",
                Cvar_VariableString("fs_basepath"));
    synthesizer = mid_init(path) == 0;
    if (!synthesizer) {
        Com_Printf("TiMidity: cannot read instrument configuration %s\n", path);
    }
}
void MusicStop() {
    OPLStop();
    musicMix.Reset();
    ClearStream(source);
    if (song) {
        mid_song_free(song);
        song = nullptr;
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
    if (synthesizer) {
        mid_exit();
    }
    synthesizer = false;
}
void MusicStart(const char *intro, const char *loop) {
    const char *repeat = loop && *loop ? loop : intro;
    const bool wantsOPL = Q_stricmp(Cvar_VariableString("s_musicBackend"), "timidity") != 0;
    const bool sameBackend = wantsOPL ? OPLLoaded() : song != nullptr;
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
               song ? mid_song_get_time(song) : OPLPosition(), loops,
               OPLLoaded() ? "opl2"
               : song      ? "timidity"
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
         count < 4 && (song || OPLLoaded() || !wave.samples.empty()); ++count) {
        int16_t pcm[8192];
        size_t size = 0;
        int rate = 44100, channels = 2;
        if (OPLLoaded()) {
            size = OPLRender(pcm, sizeof(pcm) / (sizeof(pcm[0]) * 2)) * sizeof(pcm[0]) * 2;
        } else if (song) {
            size = mid_song_read_wave(song, reinterpret_cast<sint8 *>(pcm), sizeof(pcm));
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
    if ((song || OPLLoaded() || !wave.samples.empty()) && queued > 0) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            alSourcePlay(source);
        }
    }
}
} // namespace doomaudio
