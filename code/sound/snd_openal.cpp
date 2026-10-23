// SPDX-License-Identifier: GPL-3.0-or-later
#include "../client/client.h"
#include "snd_local.h"
#include "../openal/include/AL/al.h"
#include "../openal/include/AL/alc.h"
#include "../openal/include/AL/alext.h"
#include <windows.h>
#include "../win32/win_local.h"
#include <array>
#include <vector>

// Cinematic timing remains in the engine's historical 22050-Hz sample units.
int s_soundtime, s_paintedtime, s_rawend[32];
namespace {
constexpr int SourceCount = 128, SoundCount = 4096;
struct Sound {
    char name[MAX_QPATH];
    ALuint buffer;
};
struct Voice {
    ALuint source;
    int entity = -1, channel = 0, sound = 0;
    bool loop = false, seen = false, local = false, follow = false;
    float gain = 1, range = 1250;
};
std::array<Sound, SoundCount> sounds;
std::array<Voice, SourceCount> voices;
std::array<ALuint, 32> rawSources;
vec3_t positions[MAX_GENTITIES];
struct Stream {
    ALuint source = 0;
    doomaudio::Wave wave;
    size_t cursor = 0;
    char loop[MAX_QPATH]{};
    int entity = -1, channel = 0;
    bool attenuated = false;
};
std::array<Stream, 16> streams;
bool editorPaused = false;
int editorPauseStarted = 0;
bool ReadStream(Stream &stream, const char *path) {
    void *data = nullptr;
    const int length = FS_ReadFile(path, &data);
    bool valid = false;
    if (data && length > 0 && length <= 64 * 1024 * 1024) {
        valid = doomaudio::DecodeWave(static_cast<const byte *>(data), length, stream.wave);
    }
    if (data) {
        FS_FreeFile(data);
    }
    stream.cursor = 0;
    if (!valid) {
        stream.wave = {};
        Com_Printf("OpenAL: cannot stream %s\n", path);
    }
    return valid;
}
void UpdateStreams(float gain) {
    if (editorPaused) {
        return;
    }
    for (auto &stream : streams) {
        if (!stream.source || stream.wave.samples.empty()) {
            continue;
        }
        alSourcef(stream.source, AL_GAIN, gain);
        ALint queued, processed;
        alGetSourcei(stream.source, AL_BUFFERS_QUEUED, &queued);
        alGetSourcei(stream.source, AL_BUFFERS_PROCESSED, &processed);
        for (int count = queued - processed; count < 4; ++count) {
            if (stream.cursor == stream.wave.samples.size()) {
                if (!stream.loop[0]) {
                    break;
                }
                if (!ReadStream(stream, stream.loop)) {
                    break;
                }
            }
            const size_t samples = min(size_t(8192), stream.wave.samples.size() - stream.cursor);
            doomaudio::QueuePCM(stream.source, stream.wave.samples.data() + stream.cursor,
                                static_cast<int>(samples * 2), stream.wave.rate,
                                stream.wave.channels, 2);
            stream.cursor += samples;
        }
        if (stream.attenuated && stream.entity >= 0 && stream.entity < MAX_GENTITIES) {
            alSource3f(stream.source, AL_POSITION, positions[stream.entity][0],
                       positions[stream.entity][2], -positions[stream.entity][1]);
        }
    }
}
ALCdevice *device;
ALCcontext *context;
// Entry points for the ALC_SOFT_pause_device extension. The bundled import
// library does not export them, so they are resolved at runtime instead.
LPALCDEVICEPAUSESOFT PauseDeviceSOFT = nullptr;
LPALCDEVICERESUMESOFT ResumeDeviceSOFT = nullptr;
int soundCount = 1, listener = -1;
cvar_t *volume, *musicVolume, *muted;
float globalGain = 1, fadeStartGain = 1, fadeTarget = 1;
int fadeStart, fadeEnd;
bool Ready() {
    return context != nullptr;
}
void Position(ALuint source, const vec3_t point) {
    alSource3f(source, AL_POSITION, point[0], point[2], -point[1]);
}
void Stop(Voice &voice) {
    alSourceStop(voice.source);
    alSourcei(voice.source, AL_BUFFER, 0);
    voice.entity = -1;
    voice.sound = 0;
    voice.loop = false;
    voice.follow = false;
}
Voice *Allocate(int entity, int channel) {
    Voice *available = nullptr;
    for (auto &voice : voices) {
        if (channel && voice.entity == entity && voice.channel == channel && !voice.loop) {
            Stop(voice);
            return &voice;
        }
        ALint state;
        alGetSourcei(voice.source, AL_SOURCE_STATE, &state);
        if (!available && (state != AL_PLAYING || !voice.sound)) {
            available = &voice;
        }
    }
    if (available) {
        Stop(*available);
        return available;
    }
    return nullptr;
}
void Start(const vec3_t origin, int entity, int channel, int sound, bool local) {
    if (!Ready() || sound <= 0 || sound >= soundCount || !sounds[sound].buffer) {
        return;
    }
    auto *voice = Allocate(entity, channel);
    if (!voice) {
        return;
    }
    voice->entity = entity;
    voice->channel = channel;
    voice->sound = sound;
    voice->local = local || entity == listener;
    voice->follow = !origin && !voice->local;
    voice->gain = 1;
    voice->range = 1250;
    alSourcei(voice->source, AL_SOURCE_RELATIVE, voice->local ? AL_TRUE : AL_FALSE);
    alSourcei(voice->source, AL_LOOPING, AL_FALSE);
    alSourcef(voice->source, AL_PITCH, 1);
    alSourcef(voice->source, AL_GAIN, volume->value);
    alSourcef(voice->source, AL_REFERENCE_DISTANCE, 160);
    alSourcef(voice->source, AL_MAX_DISTANCE, 1250);
    alSourcef(voice->source, AL_ROLLOFF_FACTOR, voice->local ? 0 : 1);
    vec3_t zero{};
    Position(voice->source, voice->local                            ? zero
                            : origin                                ? origin
                            : entity >= 0 && entity < MAX_GENTITIES ? positions[entity]
                                                                    : zero);
    alSourcei(voice->source, AL_BUFFER, sounds[sound].buffer);
    alSourcePlay(voice->source);
}
void PlayCommand() {
    for (int i = 1; i < Cmd_Argc(); ++i) {
        S_StartLocalSound(S_RegisterSound(Cmd_Argv(i), qfalse), CHAN_LOCAL_SOUND);
    }
}
void MusicCommand() {
    S_StartBackgroundTrack(Cmd_Argv(1), Cmd_Argc() > 2 ? Cmd_Argv(2) : Cmd_Argv(1), 0);
}
void InfoCommand() {
    Com_Printf("Doom audio: OpenAL Soft dynamic, %d sources, %d registered sounds; Doom OPL2 "
               "and TiMidity music\n",
               SourceCount, soundCount - 1);
    int playing = 0, sampleOffset = 0;
    for (const auto &voice : voices) {
        ALint state = AL_STOPPED, offset = 0;
        alGetSourcei(voice.source, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING) {
            ++playing;
            alGetSourcei(voice.source, AL_SAMPLE_OFFSET, &offset);
            sampleOffset += offset;
        }
    }
    Com_Printf("Doom audio state: editorPaused=%d voices=%d sampleOffset=%d\n", editorPaused,
               playing, sampleOffset);
}
} // namespace

void S_SetEditorPaused(qboolean paused) {
    const bool requested = paused != qfalse;
    if (requested == editorPaused) {
        return;
    }
    const int now = Sys_Milliseconds();
    editorPaused = requested;
    if (requested) {
        editorPauseStarted = now;
        if (Ready()) {
            alListenerf(AL_GAIN, 0);
            // Stop the device clock as well as output. Sources, queued buffers
            // and synthesizer positions remain intact until the editor closes.
            if (PauseDeviceSOFT) {
                PauseDeviceSOFT(device);
            }
        }
    } else {
        if (fadeEnd > fadeStart) {
            fadeStart += now - editorPauseStarted;
            fadeEnd += now - editorPauseStarted;
        }
        if (Ready()) {
            if (ResumeDeviceSOFT) {
                ResumeDeviceSOFT(device);
            }
        }
    }
}

void S_Init() {
    if (Ready()) {
        return;
    }
    volume = Cvar_Get("s_volume", "0.8", CVAR_ARCHIVE);
    musicVolume = Cvar_Get("s_musicvolume", "0.5", CVAR_ARCHIVE);
    muted = Cvar_Get("s_muteWhenUnfocused", "0", CVAR_ARCHIVE);
    if (!Cvar_Get("s_initsound", "1", 0)->integer) {
        return;
    }
    device = alcOpenDevice(nullptr);
    if (!device) {
        Com_Printf("OpenAL: could not open an audio device\n");
        return;
    }
    context = alcCreateContext(device, nullptr);
    if (!context || !alcMakeContextCurrent(context)) {
        if (context) {
            alcDestroyContext(context);
        }
        context = nullptr;
        alcCloseDevice(device);
        device = nullptr;
        Com_Printf("OpenAL: could not create audio context\n");
        return;
    }
    alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    alDopplerFactor(0);
    // The bundled import library does not export the pause/resume extension
    // entry points, so fetch them by name from the loaded OpenAL runtime.
    PauseDeviceSOFT = reinterpret_cast<LPALCDEVICEPAUSESOFT>(
        alcGetProcAddress(device, "alcDevicePauseSOFT"));
    ResumeDeviceSOFT = reinterpret_cast<LPALCDEVICERESUMESOFT>(
        alcGetProcAddress(device, "alcDeviceResumeSOFT"));
    if (editorPaused) {
        alListenerf(AL_GAIN, 0);
        if (PauseDeviceSOFT) {
            PauseDeviceSOFT(device);
        }
    }
    for (auto &voice : voices) {
        alGenSources(1, &voice.source);
    }
    Cvar_Set("s_backend", "OpenAL Soft (dynamic)");
    Cmd_AddCommand("play", PlayCommand);
    Cmd_AddCommand("music", MusicCommand);
    Cmd_AddCommand("s_info", InfoCommand);
    Cmd_AddCommand("stopsound", S_StopAllSounds);
    Cmd_AddCommand("s_musicinfo", doomaudio::MusicInfo);
    doomaudio::MusicInit();
    InfoCommand();
}
void S_Shutdown() {
    if (!Ready()) {
        return;
    }
    S_StopAllSounds();
    doomaudio::MusicShutdown();
    for (auto &voice : voices) {
        alDeleteSources(1, &voice.source);
        voice = {};
    }
    for (auto &source : rawSources) {
        if (source) {
            alDeleteSources(1, &source);
        }
        source = 0;
    }
    for (auto &stream : streams) {
        if (stream.source) {
            alDeleteSources(1, &stream.source);
        }
        stream = {};
    }
    for (int i = 1; i < soundCount; ++i) {
        if (sounds[i].buffer) {
            alDeleteBuffers(1, &sounds[i].buffer);
        }
    }
    sounds = {};
    soundCount = 1;
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    alcCloseDevice(device);
    context = nullptr;
    device = nullptr;
    for (const char *command : {"play", "music", "s_info", "stopsound", "s_musicinfo"}) {
        Cmd_RemoveCommand(command);
    }
}
sfxHandle_t S_RegisterSound(const char *name, qboolean) {
    if (!Ready() || !name || !*name || strlen(name) >= MAX_QPATH) {
        return 0;
    }
    for (int i = 1; i < soundCount; ++i) {
        if (!Q_stricmp(sounds[i].name, name)) {
            return i;
        }
    }
    if (soundCount >= SoundCount) {
        Com_Printf("OpenAL sound limit reached: %s\n", name);
        return 0;
    }
    const int handle = soundCount++;
    auto &sound = sounds[handle];
    Q_strncpyz(sound.name, name, sizeof(sound.name));
    void *data = nullptr;
    const int length = FS_ReadFile(name, &data);
    if (length <= 0 || length > 64 * 1024 * 1024) {
        if (data) {
            FS_FreeFile(data);
        }
        return handle;
    }
    doomaudio::Wave wave;
    const bool decoded = doomaudio::DecodeWave(static_cast<byte *>(data), length, wave);
    FS_FreeFile(data);
    if (!decoded) {
        Com_Printf("OpenAL: unsupported or malformed WAV: %s\n", name);
        return handle;
    }
    alGenBuffers(1, &sound.buffer);
    alBufferData(sound.buffer, wave.channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16,
                 wave.samples.data(), static_cast<ALsizei>(wave.samples.size() * 2), wave.rate);
    if (alGetError() != AL_NO_ERROR) {
        alDeleteBuffers(1, &sound.buffer);
        sound.buffer = 0;
        Com_Printf("OpenAL: cannot upload %s\n", name);
    }
    return handle;
}
void S_StartSound(vec3_t origin, int entity, int channel, sfxHandle_t sound) {
    Start(origin, entity, channel, sound, false);
}
void S_StartSoundEx(vec3_t origin, int entity, int channel, sfxHandle_t sound, int) {
    Start(origin, entity, channel, sound, false);
}
void S_StartLocalSound(sfxHandle_t sound, int channel) {
    Start(nullptr, listener, channel, sound, true);
}
void S_UpdateEntityPosition(int entity, const vec3_t origin) {
    if (entity >= 0 && entity < MAX_GENTITIES) {
        VectorCopy(origin, positions[entity]);
    }
}
void S_Respatialize(int entity, const vec3_t origin, vec3_t axis[3], int) {
    if (!Ready()) {
        return;
    }
    listener = entity;
    alListener3f(AL_POSITION, origin[0], origin[2], -origin[1]);
    const float orientation[] = {axis[0][0], axis[0][2], -axis[0][1],
                                 axis[2][0], axis[2][2], -axis[2][1]};
    alListenerfv(AL_ORIENTATION, orientation);
}
void S_ClearLoopingSounds() {
    for (auto &voice : voices) {
        voice.seen = false;
    }
}
void S_AddLoopingSound(int entity, const vec3_t origin, const vec3_t, const int range,
                       sfxHandle_t sound, int gain) {
    if (!Ready() || !sound || sound >= soundCount) {
        return;
    }
    Voice *voice = nullptr;
    for (auto &candidate : voices) {
        if (candidate.loop && candidate.entity == entity && candidate.sound == sound) {
            voice = &candidate;
            break;
        }
    }
    if (!voice) {
        Start(origin, entity, 0, sound, false);
        for (auto &candidate : voices) {
            if (!candidate.loop && candidate.entity == entity && candidate.sound == sound) {
                voice = &candidate;
                break;
            }
        }
    }
    if (!voice) {
        return;
    }
    voice->loop = voice->seen = true;
    voice->gain = min(255, max(0, gain & 255)) / 255.0f;
    alSourcei(voice->source, AL_LOOPING, AL_TRUE);
    Position(voice->source, origin);
    alSourcef(voice->source, AL_MAX_DISTANCE, range > 0 ? range : 1250);
}
void S_AddRealLoopingSound(int entity, const vec3_t origin, const vec3_t velocity, const int range,
                           sfxHandle_t sound) {
    S_AddLoopingSound(entity, origin, velocity, range, sound, 255);
}
void S_StopLoopingSound(int entity) {
    if (Ready()) {
        for (auto &voice : voices) {
            if (voice.loop && voice.entity == entity) {
                Stop(voice);
            }
        }
    }
}
void S_StopAllSounds() {
    if (!Ready()) {
        return;
    }
    for (auto &voice : voices) {
        Stop(voice);
    }
    for (auto source : rawSources) {
        doomaudio::ClearStream(source);
    }
    for (size_t i = 0; i < streams.size(); ++i) {
        S_StopStreamingSound(static_cast<int>(i));
    }
    doomaudio::MusicStop();
}
void S_ClearSounds(qboolean streaming, qboolean music) {
    if (!Ready()) {
        return;
    }
    for (auto &voice : voices) {
        Stop(voice);
    }
    if (streaming) {
        for (auto source : rawSources) {
            doomaudio::ClearStream(source);
        }
        for (size_t i = 0; i < streams.size(); ++i) {
            S_StopStreamingSound(static_cast<int>(i));
        }
    }
    if (music) {
        doomaudio::MusicStop();
    }
}
void S_Update() {
    if (!Ready() || editorPaused) {
        return;
    }
    const int now = Sys_Milliseconds();
    if (fadeEnd > fadeStart) {
        const float fraction = min(1.0f, max(0.0f, float(now - fadeStart) / (fadeEnd - fadeStart)));
        globalGain = fadeStartGain + (fadeTarget - fadeStartGain) * fraction;
    }
    alListenerf(AL_GAIN, max(0.0f, globalGain) * ((muted->integer && !g_wv.activeApp) ? 0 : 1));
    for (auto &voice : voices) {
        if (voice.loop && !voice.seen) {
            Stop(voice);
        }
        if (voice.follow && voice.entity >= 0 && voice.entity < MAX_GENTITIES) {
            Position(voice.source, positions[voice.entity]);
        }
        alSourcef(voice.source, AL_GAIN, max(0.0f, volume->value) * voice.gain);
    }
    UpdateStreams(max(0.0f, volume->value));
    doomaudio::MusicUpdate(max(0.0f, musicVolume->value));
    s_soundtime = s_paintedtime = static_cast<int>(static_cast<int64_t>(now) * 22050 / 1000);
}
void S_RawSamples(int samples, int rate, int width, int channels, const byte *data, float left,
                  float right, int index) {
    if (!Ready() || index < 0 || index >= 32 || samples <= 0 || samples > 262144 || rate < 4000 ||
        rate > 192000 || (width != 1 && width != 2) || (channels != 1 && channels != 2) || !data) {
        return;
    }
    if (!rawSources[index]) {
        rawSources[index] = doomaudio::NewStreamSource();
    }
    alSourcef(rawSources[index], AL_GAIN, max(0.0f, (left + right) * 0.5f));
    doomaudio::QueuePCM(rawSources[index], data, samples * width * channels, rate, channels, width);
    s_rawend[index] = max(s_soundtime, s_rawend[index]) + samples * 22050 / rate;
}
void S_StartBackgroundTrack(const char *intro, const char *loop, int) {
    if (Ready()) {
        doomaudio::MusicStart(intro, loop);
    }
}
void S_StopBackgroundTrack() {
    if (Ready()) {
        doomaudio::MusicStop();
    }
}
void S_QueueBackgroundTrack(const char *loop) {
    S_StartBackgroundTrack(loop, loop, 0);
}
void S_FadeAllSounds(float target, int time) {
    fadeStartGain = globalGain;
    fadeTarget = max(0.0f, min(1.0f, target));
    fadeStart = Sys_Milliseconds();
    fadeEnd = fadeStart + max(0, time);
    if (!time) {
        globalGain = fadeTarget;
    }
}
void S_FadeStreamingSound(float target, int time, int index) {
    if (index < 0) {
        S_FadeAllSounds(target, time);
    } else if (Ready() && index < 32 && rawSources[index]) {
        alSourcef(rawSources[index], AL_GAIN, max(0.0f, target));
    }
}
void S_StartStreamingSound(const char *intro, const char *loop, int entity, int channel,
                           int attenuation) {
    if (!Ready() || !intro || !*intro) {
        return;
    }
    S_StopEntStreamingSound(entity);
    for (auto &stream : streams) {
        ALint queued = 0;
        if (stream.source) {
            alGetSourcei(stream.source, AL_BUFFERS_QUEUED, &queued);
        }
        if (stream.wave.samples.empty() ||
            (!stream.loop[0] && stream.cursor == stream.wave.samples.size() && !queued)) {
            if (!stream.source) {
                stream.source = doomaudio::NewStreamSource();
            }
            doomaudio::ClearStream(stream.source);
            stream.entity = entity;
            stream.channel = channel;
            stream.attenuated = attenuation != 0;
            Q_strncpyz(stream.loop, loop ? loop : "", sizeof(stream.loop));
            alSourcei(stream.source, AL_SOURCE_RELATIVE, attenuation ? AL_FALSE : AL_TRUE);
            alSourcef(stream.source, AL_ROLLOFF_FACTOR, attenuation ? 1.0f : 0.0f);
            alSourcef(stream.source, AL_REFERENCE_DISTANCE, 160);
            alSourcef(stream.source, AL_MAX_DISTANCE, 1250);
            ReadStream(stream, intro);
            UpdateStreams(max(0.0f, volume->value));
            return;
        }
    }
    Com_Printf("OpenAL: no free streaming channel for %s\n", intro);
}
void S_StopStreamingSound(int index) {
    if (!Ready() || index < 0 || index >= static_cast<int>(streams.size())) {
        return;
    }
    auto &stream = streams[index];
    doomaudio::ClearStream(stream.source);
    stream.wave = {};
    stream.loop[0] = 0;
}
void S_StopEntStreamingSound(int entity) {
    if (!Ready()) {
        return;
    }
    for (size_t i = 0; i < streams.size(); ++i) {
        if (streams[i].entity == entity) {
            S_StopStreamingSound(static_cast<int>(i));
        }
    }
}
void S_BeginRegistration() {
    if (!Ready()) {
        S_Init();
    }
    globalGain = 1;
    fadeStart = fadeEnd = 0;
}
void S_DisableSounds() {
    S_StopAllSounds();
}
void S_UpdateThread() {
} // OpenAL owns its mixing thread; engine filesystem work stays on the main thread.
void S_DisplayFreeMemory() {
    InfoCommand();
}
int S_GetVoiceAmplitude(int) {
    return 0;
}
void SNDDMA_Activate() {
} // OpenAL retains the device across focus changes.
// Sound buffers are device-owned rather than the old shared DMA ring.
void S_ClearSoundBuffer(qboolean killStreaming) {
    S_ClearSounds(killStreaming, killStreaming);
}
