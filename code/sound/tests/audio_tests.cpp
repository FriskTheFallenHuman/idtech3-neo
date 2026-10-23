// SPDX-License-Identifier: GPL-3.0-or-later
#define AL_ALEXT_PROTOTYPES
#include "../snd_local.h"
#include "../../openal/include/AL/al.h"
#include "../../openal/include/AL/alc.h"
#include "../../openal/include/AL/alext.h"
#include "../../timidy/src/timidity.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
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
        Check(mid_init("base/sound/timidity/timidity.cfg") == 0,
              "TiMidity patch configuration loads");
        int scores = 0;
        for (const auto &entry : std::filesystem::directory_iterator("base/music/doom")) {
            if (entry.path().extension() != ".mid") {
                continue;
            }
            auto midi = Read(entry.path());
            auto *stream = mid_istream_open_mem(midi.data(), midi.size());
            MidSongOptions options{44100, MID_AUDIO_S16LSB, 2, 0, 4096, 0};
            auto *song = mid_song_load(stream, &options);
            mid_istream_close(stream);
            Check(song && mid_song_get_total_time(song) > 1000,
                  "Converted MIDI parses with a valid duration");
            mid_song_start(song);
            std::vector<int16_t> output(44100 * 2 * 3);
            const auto count = mid_song_read_wave(song, reinterpret_cast<sint8 *>(output.data()),
                                                  output.size() * 2);
            Check(count > 0 && std::any_of(output.begin(), output.end(),
                                           [](int16_t sample) { return sample != 0; }),
                  "TiMidity produces audible PCM for every Doom score");
            mid_song_seek(song, mid_song_get_total_time(song) - 500);
            Check(mid_song_read_wave(song, reinterpret_cast<sint8 *>(output.data()), 4096) > 0,
                  "TiMidity seek near track end");
            mid_song_start(song);
            Check(mid_song_get_time(song) == 0, "TiMidity loop restarts at beginning");
            mid_song_free(song);
            ++scores;
        }
        mid_exit();
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
        std::printf("Audio passed: WAV bounds/PCM decoding, %d TiMidity scores/seeks/loops and "
                    "static OpenAL spatial output.\n",
                    scores);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
