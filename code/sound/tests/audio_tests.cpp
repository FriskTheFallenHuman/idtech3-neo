// SPDX-License-Identifier: GPL-3.0-or-later
#define AL_ALEXT_PROTOTYPES
#include "../snd_local.h"
#include "../../openal/include/AL/al.h"
#include "../../openal/include/AL/alc.h"
#include "../../openal/include/AL/alext.h"
#include "../snd_fluidsynth.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void Check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
std::vector<uint8_t> Read(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    Check(input.good(), "Audio fixture is missing");
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
}
} // namespace
int main() {
    try {
        auto bytes = Read("base/sound/doom/weapons/pistol.wav");
        doomaudio::Wave wave;
        Check(doomaudio::DecodeWave(bytes.data(), bytes.size(), wave),
              "Original odd-sized PCM8 WAV decodes");
        Check(wave.channels == 1 && wave.rate == 11025 && wave.samples.size() == 5661,
              "Original PCM metadata");
        auto invalid = bytes;
        invalid[40] = 0xFF;
        invalid[41] = 0xFF;
        invalid[42] = 0xFF;
        invalid[43] = 0x7F;
        Check(!doomaudio::DecodeWave(invalid.data(), invalid.size(), wave),
              "Oversized chunk rejects");
        Check(!doomaudio::DecodeWave(bytes.data(), 20, wave) &&
                  !doomaudio::DecodeWave(nullptr, 0, wave),
              "Truncated input rejects");
        auto &fs = doomaudio::fluidsynth;
        Check(fs.Load(), "FluidSynth loads from libfluidsynth-3.dll");
        auto *settings = fs.new_settings();
        Check(settings && fs.settings_setnum(settings, "synth.sample-rate", 44100.0) == FLUID_OK &&
                  fs.settings_setstr(settings, "player.timing-source", "sample") == FLUID_OK,
              "FluidSynth settings apply");
        auto *synth = fs.new_synth(settings);
        Check(synth && fs.sfload(synth, "base/sound/timidity/TimGM6mb.sf2", 1) >= 0,
              "TimGM6mb SoundFont loads");
        int scores = 0;
        for (const auto &entry : std::filesystem::directory_iterator("base/music/doom")) {
            if (entry.path().extension() != ".mid") {
                continue;
            }
            auto midi = Read(entry.path());
            auto *player = fs.new_player(synth);
            Check(player && fs.player_add_mem(player, midi.data(), midi.size()) == FLUID_OK &&
                      fs.player_get_total_ticks(player) > 0,
                  "Converted MIDI parses with a valid duration");
            Check(fs.player_play(player) == FLUID_OK, "FluidSynth playback starts");
            std::vector<int16_t> output(44100 * 2 * 3);
            // The player dispatches events on the render thread's sample
            // clock, so render in small chunks and give it time to run.
            for (size_t offset = 0; offset < output.size() / 2; offset += 512) {
                const int frames =
                    static_cast<int>(std::min<size_t>(512, output.size() / 2 - offset));
                fs.write_s16(synth, frames, output.data() + offset * 2, 0, 2,
                              output.data() + offset * 2 + 1, 0, 2);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            Check(std::any_of(output.begin(), output.end(),
                             [](int16_t sample) { return sample != 0; }),
                  "FluidSynth produces audible PCM for every Doom score");
            fs.delete_player(player);
            ++scores;
        }
        fs.delete_synth(synth);
        fs.delete_settings(settings);
        fs.Unload();
        Check(scores == 32, "All original Doom scores present");
        auto *device = alcLoopbackOpenDeviceSOFT(nullptr);
        Check(device != nullptr, "Statically linked OpenAL loopback device opens");
        const ALCint attributes[] = {
            ALC_FREQUENCY,  44100, ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT,
            ALC_SHORT_SOFT, 0};
        auto *context = alcCreateContext(device, attributes);
        Check(context && alcMakeContextCurrent(context), "OpenAL context creates");
        ALuint buffer, source;
        alGenBuffers(1, &buffer);
        alGenSources(1, &source);
        alBufferData(buffer, AL_FORMAT_MONO16, wave.samples.data(),
                     static_cast<ALsizei>(wave.samples.size() * 2), wave.rate);
        alSourcei(source, AL_BUFFER, buffer);
        alSource3f(source, AL_POSITION, -1, 0, -1);
        alSourcePlay(source);
        std::vector<int16_t> mixed(44100 * 2);
        alcRenderSamplesSOFT(device, mixed.data(), 44100);
        long long left = 0, right = 0;
        for (size_t i = 0; i < mixed.size(); i += 2) {
            left += std::abs(int(mixed[i]));
            right += std::abs(int(mixed[i + 1]));
        }
        Check(left > right && right > 0 && alGetError() == AL_NO_ERROR,
              "OpenAL renders spatial PCM to the correct stereo side");
        alDeleteSources(1, &source);
        alDeleteBuffers(1, &buffer);
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        alcCloseDevice(device);
        std::printf("Audio passed: WAV bounds/PCM decoding, %d FluidSynth scores and "
                    "static OpenAL spatial output.\n",
                    scores);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

