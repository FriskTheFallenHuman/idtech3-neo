// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
namespace doomaudio {
class MusicMix {
  public:
    void Reset();
    void Configure(int rate, double gainDb, double bassDb);
    void Process(int16_t *pcm, size_t frames, int channels);

  private:
    struct State {
        double first = 0, second = 0;
    };
    State state[2]{};
    int sampleRate = 0;
    double gainDb = 0, bassDb = 0, gain = 1;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};
} // namespace doomaudio
