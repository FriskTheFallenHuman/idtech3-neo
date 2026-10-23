// SPDX-License-Identifier: GPL-3.0-or-later
#define AL_ALEXT_PROTOTYPES
#include "client/client.h"
#include "sound/snd_local.h"
#include "sound/snd_musicmix.h"
#include "openal/include/AL/al.h"
#include "openal/include/AL/alc.h"
#include "openal/include/AL/alext.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <limits>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
std::string report;
char backend[16] = "opl";
} // namespace
void QDECL Com_Printf(const char *format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    report = text;
}
void QDECL Com_Error(int, const char *message, ...) {
    throw std::runtime_error(message);
}
cvar_t *Cvar_Get(const char *, const char *, int) {
    return nullptr;
}
float Cvar_VariableValue(const char *name) {
    return !std::strcmp(name, "s_musicGain") ? 6.0f : 4.0f;
}
char *Cvar_VariableString(const char *name) {
    static char root[] = ".";
    return !std::strcmp(name, "s_musicBackend") ? backend : root;
}
int FS_ReadFile(const char *name, void **buffer) {
    std::ifstream input(std::string("base/") + name, std::ios::binary | std::ios::ate);
    if (!input.good()) {
        *buffer = nullptr;
        return -1;
    }
    const int length = static_cast<int>(input.tellg());
    input.seekg(0);
    *buffer = malloc(length);
    input.read(static_cast<char *>(*buffer), length);
    return length;
}
void FS_FreeFile(void *buffer) {
    free(buffer);
}
void Check(bool condition, const char *why) {
    if (!condition)
        throw std::runtime_error(why);
}
void CheckMusicMix() {
    constexpr size_t frames = 44100;
    std::vector<int16_t> input(frames * 2);
    doomaudio::MusicMix mix;
    for (int frequency : {55, 1000}) {
        for (size_t frame = 0; frame < frames; ++frame) {
            input[frame * 2] = static_cast<int16_t>(
                2000 * std::
sin(2 * 3.14159265358979323846 * frequency * frame / 44100));
            input[frame * 2 + 1] = 0;
        }
        auto output = input;
        mix.Reset();
        mix.Configure(44100, 0, 4);
        mix.Process(output.data(), frames, 2);
        double originalEnergy = 0, mixedEnergy = 0;
        for (size_t frame = 4410; frame < frames; ++frame) {
            originalEnergy += double(input[frame * 2]) * input[frame * 2];
            mixedEnergy += double(output[frame * 2]) * output[frame * 2];
            Check(output[frame * 2 + 1] == 0, "Music EQ must not leak across stereo channels");
        }
        const double decibels = 10 * std::log10(mixedEnergy / originalEnergy);
        Check(frequency == 55 ? decibels > 3.8 && decibels < 4.2 : std::abs(decibels) < 0.1,
              "Bass shelf boosts 55-Hz weight while preserving 1-kHz balance");
        auto split = input;
        mix.Reset();
        for (size_t offset = 0; offset < frames;) {
            const size_t count = std::min<size_t>(137, frames - offset);
            mix.Process(split.data() + offset * 2, count, 2);
            offset += count;
        }
        Check(split == output, "Music EQ is independent of render chunk size");
    }
    auto output = input;
    mix.Configure(44100, 0, 0);
    mix.Process(output.data(), frames, 2);
    Check(output == input, "Zero bass and gain provide exact uncolored playback");
    mix.Configure(44100, std::numeric_limits<double>::quiet_NaN(), 0);
    mix.Process(output.data(), frames, 2);
    Check(output == input, "Nonfinite settings fall back safely");
    std::fill(output.begin(), output.end(), 32767);
    mix.Reset();
    mix.Configure(44100, 12, 9);
    mix.Process(output.data(), frames, 2);
    Check(std::all_of(output.begin(), output.end(), [](int16_t sample) { return sample > 0; }),
          "Maximum gain and bass must never wrap or invert loud PCM");
    std::fill(output.begin(), output.end(), -32768);
    mix.Reset();
    mix.Process(output.data(),
 frames, 2);
    Check(std::all_of(output.begin(), output.end(), [](int16_t sample) { return sample < 0; }),
          "Negative peaks must remain negative under maximum boost");
    std::fill(output.begin(), output.end(), 0);
    mix.Reset();
    mix.Process(output.data(), frames, 2);
    Check(std::all_of(output.begin(), output.end(), [](int16_t sample) { return sample == 0; }),
          "Track reset clears all filter history");
}
int main(int argc, char **argv) {
    try {
        CheckMusicMix();
        if (argc > 1) {
            Q_strncpyz(backend, argv[1], sizeof(backend));
        }
        auto *device = alcLoopbackOpenDeviceSOFT(nullptr);
        Check(device, "OpenAL loopback device");
        const ALCint attributes[] = {
            ALC_FREQUENCY,  44100, ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT,
            ALC_SHORT_SOFT, 0};
        auto *context = alcCreateContext(device, attributes);
        Check(context && alcMakeContextCurrent(context), "OpenAL context");
        doomaudio::MusicInit();
        doomaudio::MusicStart("music/doom/d_intro.mid", "music/doom/d_intro.mid");
        std::vector<int16_t> output(4096 * 2);
        long long energy = 0;
        for (int i = 0; i < 250; ++i) {
            // The UI may request the same track repeatedly; its timeline must advance.
            doomaudio::MusicStart("music/doom/d_intro.mid", "music/doom/d_intro.mid");
            doomaudio::MusicUpdate(0.5f);
            alcRenderSamplesSOFT(device, output.data(), 4096);
            for (auto sample : output)
                energy += abs(int(sample));
        }
        doomaudio::MusicUpdate(0.5f);
        doomaudio::MusicInfo();
        int playing, queued, loops;
        unsigned position;
        Check(sscanf(report.c_str(),
                     "Doom music: track=%*s playing=%d queued=%d position=%u loops=%d", &playing,
                     &queued, &position, &loops) == 4 &&
                  playing && queued > 0 && loops > 0 &
& energy > 0,
              "Real music streaming must remain audible through title-track loops and repeated "
              "requests");
        std::vector<int16_t> stall(44100 * 2);
        alcRenderSamplesSOFT(device, stall.data(), 44100);
        doomaudio::MusicUpdate(0.5f);
        doomaudio::MusicInfo();
        Check(report.find("playing=1") != std::string::npos && alGetError() == AL_NO_ERROR,
              "Music recovers after queue starvation");
        doomaudio::MusicStop();
        doomaudio::MusicInfo();
        Check(report.find("playing=0") != std::string::npos, "Explicit music stop");
        doomaudio::MusicStart("music/doom/d_e1m2.mid", "music/doom/d_e1m2.mid");
        doomaudio::MusicUpdate(0.5f);
        alcRenderSamplesSOFT(device, output.data(), 4096);
        doomaudio::MusicInfo();
        Check(report.find("playing=1") != std::string::npos, "Destination music starts after stop");
        Q_strncpyz(backend, !std::strcmp(backend, "fluidsynth") ? "opl" : "fluidsynth",
                   sizeof(backend));
        doomaudio::MusicStart("music/doom/d_e1m2.mid", "music/doom/d_e1m2.mid");
        doomaudio::MusicUpdate(0.5f);
        alcRenderSamplesSOFT(device, output.data(), 4096);
        doomaudio::MusicInfo();
        const char *expectedBackend =
            !std::strcmp(backend, "fluidsynth") ? "backend=fluidsynth" : "backend=opl2";
        Check(report.find("playing=1") != std::string::npos &&
                  report.find(expectedBackend) != std::string::npos,
              "Changing backend restarts the same track with the requested synthesizer");
        doomaudio::MusicShutdown();
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        alcCloseDevice(device);
        std::puts("Real music backend passed: sustained playback, repeated requests, complete "
                  "loops, starvation recovery, stop and level-track replacement.");
    } catch (const std::exception &error) {
        std::fprintf(stder
r, "%s\n", error.what());
        return 1;
    }
}

