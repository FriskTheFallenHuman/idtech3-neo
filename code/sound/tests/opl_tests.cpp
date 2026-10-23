// SPDX-License-Identifier: GPL-3.0-or-later
#include "../snd_opl.h"
#include "../snd_musicmix.h"
#include "../opl/doom_opl.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
namespace {
void Check(bool value, const char *why) {
    if (!value) {
        throw std::runtime_error(why);
    }
}
std::vector<uint8_t> Read(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    Check(input.good(), "OPL fixture missing");
    return {std::istreambuf_iterator<char>(input), {}};
}
int Crossings(const std::vector<int16_t> &pcm) {
    int count = 0;
    for (size_t i = 4098; i < pcm.size(); i += 2) {
        if (pcm[i - 2] <= 0 && pcm[i] > 0) {
            ++count;
        }
    }
    return count;
}
void Pitch() {
    // A controlled single-sine carrier gives an independent sample-clock/pitch
    // check without relying on the frequency lookup table under test.
    std::vector<uint8_t> bank(8 + 175 * 36);
    std::memcpy(bank.data(), "#OPL_II#", 8);
    auto *voice = bank.data() + 12;
    voice[0] = 0x20;
    voice[1] = 0xf0;
    voice[2] = 0x0f;
    voice[5] = 63;
    voice[7] = 0x20;
    voice[8] = 0xf0;
    voice[9] = 0x0f;
    struct Tone {
        uint8_t note;
        double frequency;
    };
    const Tone tones[] = {{33, 55}, {45, 110}, {69, 440}, {81, 880}};
    constexpr size_t frames = 44100 * 4;
    std::vector<int16_t> pcm(frames * 2);
    for (const auto &tone : tones) {
        Check(DoomOPL_Reset(bank.data(), bank.size()), "Synthetic GENMIDI bank");
        DoomOPL_Event(0x90, tone.note, 127);
        DoomOPL_Render(pcm.data(), frames);
        const double frequency = Crossings(pcm) * 44100.0 / (frames - 2049);
        std::printf("OPL tone %d: %.3f Hz, crossings=%d, max=%d\n", tone.note, frequency,
                    Crossings(pcm), *std::max_element(pcm.begin(), pcm.end()));
        Check(frequency > tone.frequency * 0.99 && frequency < tone.frequency * 1.01,
              "OPL bass and treble notes must render at the intended pitch and sample rate");
    }
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 3 && std::strcmp(argv[1], "--render") == 0) {
            const auto bank = Read("base/sound/doom/genmidi.op2");
            const auto midi = Read("base/music/doom/d_e1m1.mid");
            Check(doomaudio::OPLLoad(midi.data(), midi.size(), bank.data(), bank.size()),
                  "Preview score");
            std::vector<int16_t> pcm(44100 * 2 * 12);
            Check(doomaudio::OPLRender(pcm.data(), 44100 * 12) == 44100 * 12, "Preview render");
            doomaudio::MusicMix mix;
            mix.Configure(44100, 6, 4);
            mix.Process(pcm.data(), 44100 * 12, 2);
            std::ofstream output(argv[2], std::ios::binary);
            const auto number = [&](uint32_t value, int bytes) {
                for (int i = 0; i < bytes; ++i) {
                    output.put(static_cast<char>((value >> (i * 8)) & 255));
                }
            };
            const auto size = static_cast<uint32_t>(pcm.size() * sizeof(pcm[0]));
            output.write("RIFF", 4);
            number(size + 36, 4);
            output.write("WAVEfmt ", 8);
            number(16, 4);
            number(1, 2);
            number(2, 2);
            number(44100, 4);
            number(44100 * 4, 4);
            number(4, 2);
            number(16, 2);
            output.write("data", 4);
            number(size, 4);
            output.write(reinterpret_cast<const char *>(pcm.data()), size);
            Check(output.good(), "Writing preview audio");
            return 0;
        }
        Pitch();
        auto bank = Read("base/sound/doom/genmidi.op2");
        auto midi = Read("base/music/doom/d_e1m1.mid");
        Check(!doomaudio::OPLLoad(nullptr, 0, bank.data(), bank.size()) &&
                  !doomaudio::OPLLoad(midi.data(), midi.size(), bank.data(), 10),
              "Truncated inputs reject");
        auto invalid = midi;
        invalid[18] = 0x7f;
        Check(!doomaudio::OPLLoad(invalid.data(), invalid.size(), bank.data(), bank.size()),
              "Out-of-bounds track rejects");
        invalid = midi;
        invalid[12] = 0x80;
        Check(!doomaudio::OPLLoad(invalid.data(), invalid.size(), bank.data(), bank.size()),
              "Unsupported SMPTE division rejects");
        // Two MIDI tracks share tempo changes from track zero. 100 ticks at
        // 500000 us/quarter plus 100 at 1000000 with 100 PPQN is 1.5 seconds.
        const uint8_t multitrack[] = {'M', 'T', 'h', 'd', 0,   0,   0,  6,  0,  1,   0,   2,   0,
                                      100, 'M', 'T', 'r', 'k', 0,   0,  0,  18, 0,   255, 81,  3,
                                      7,   161, 32,  100, 255, 81,  3,  15, 66, 64,  100, 255, 47,
                                      0,   'M', 'T', 'r', 'k', 0,   0,  0,  16, 0,   192, 0,   0,
                                      144, 69,  127, 129, 72,  128, 69, 0,  0,  255, 47,  0};
        Check(doomaudio::OPLLoad(multitrack, sizeof(multitrack), bank.data(), bank.size()) &&
                  doomaudio::OPLDuration() == 1500,
              "Format-1 tracks share exact tempo changes");
        std::vector<int16_t> ending(70000 * 2);
        Check(doomaudio::OPLRender(ending.data(), 70000) == 66150 &&
                  doomaudio::OPLPosition() == 1500 && !doomaudio::OPLRender(ending.data(), 1),
              "End of track stops at the exact scheduled sample");
        Check(doomaudio::OPLLoad(midi.data(), midi.size(), bank.data(), bank.size()) &&
                  doomaudio::OPLDuration() == 96000,
              "E1M1 original score duration");
        std::vector<int16_t> whole(44100 * 2), split(whole.size());
        Check(doomaudio::OPLRender(whole.data(), 44100) == 44100, "One-second sample clock");
        Check(doomaudio::OPLLoad(midi.data(), midi.size(), bank.data(), bank.size()),
              "Deterministic restart");
        for (size_t offset = 0; offset < 44100;) {
            const size_t count = std::min(size_t(137), size_t(44100) - offset);
            Check(doomaudio::OPLRender(split.data() + offset * 2, count) == count,
                  "Partitioned rendering");
            offset += count;
        }
        Check(whole == split && doomaudio::OPLPosition() == 1000,
              "Output and tempo must not depend on frame/chunk size");
        int scores = 0;
        for (const auto &entry : std::filesystem::directory_iterator("base/music/doom")) {
            if (entry.path().extension() != ".mid") {
                continue;
            }
            midi = Read(entry.path());
            Check(doomaudio::OPLLoad(midi.data(), midi.size(), bank.data(), bank.size()),
                  "Every original score parses for OPL");
            std::vector<int16_t> output(44100 * 2 * 3);
            Check(doomaudio::OPLRender(output.data(), 44100 * 3) == 44100 * 3 &&
                      std::any_of(output.begin(), output.end(),
                                  [](int16_t sample) { return sample != 0; }),
                  "Every original score produces audible FM output");
            ++scores;
        }
        Check(scores == 32, "Original score inventory");
        doomaudio::OPLStop();
        Check(!doomaudio::OPLLoaded() && !doomaudio::OPLRender(whole.data(), 16), "Stopped output");
        std::puts(
            "Doom OPL passed: bass/treble pitch, sample clock, frame-independent PCM, original "
            "duration, all 32 GENMIDI scores and malformed input.");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
