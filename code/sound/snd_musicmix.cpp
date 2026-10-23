// SPDX-License-Identifier: GPL-3.0-or-later
#include "snd_musicmix.h"
#include <algorithm>
#include <cmath>
namespace doomaudio {
void MusicMix::Reset() {
    state[0] = {};
    state[1] = {};
}
void MusicMix::Configure(int rate, double requestedGain, double requestedBass) {
    requestedGain = std::isfinite(requestedGain) ? std::clamp(requestedGain, -12.0, 12.0) : 0;
    requestedBass = std::isfinite(requestedBass) ? std::clamp(requestedBass, 0.0, 9.0) : 0;
    rate = std::clamp(rate, 8000, 192000);
    if (rate == sampleRate && requestedGain == gainDb && requestedBass == bassDb) {
        return;
    }
    if (sampleRate != rate) {
        Reset();
    }
    sampleRate = rate;
    gainDb = requestedGain;
    bassDb = requestedBass;
    gain = std::pow(10.0, gainDb / 20.0);
    // RBJ low shelf, 180-Hz midpoint and monotonic slope S=1:
    // https://www.w3.org/TR/audio-eq-cookbook/#low-shelf
    const double amplitude = std::pow(10.0, bassDb / 40.0);
    const double omega = 2.0 * 3.14159265358979323846 * 180.0 / sampleRate;
    const double cosine = std::cos(omega);
    const double beta = std::sin(omega) * std::sqrt(2.0 * amplitude);
    const double sum = amplitude + 1, difference = amplitude - 1;
    const double denominator = sum + difference * cosine + beta;
    b0 = amplitude * (sum - difference * cosine + beta) / denominator;
    b1 = 2 * amplitude * (difference - sum * cosine) / denominator;
    b2 = amplitude * (sum - difference * cosine - beta) / denominator;
    a1 = -2 * (difference + sum * cosine) / denominator;
    a2 = (sum + difference * cosine - beta) / denominator;
}
void MusicMix::Process(int16_t *pcm, size_t frames, int channels) {
    if (!pcm || channels < 1 || channels > 2 || (gainDb == 0 && bassDb == 0)) {
        return;
    }
    for (size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            auto &filter = state[channel];
            auto &sample = pcm[frame * channels + channel];
            const double input = sample;
            const double filtered = b0 * input + filter.first;
            filter.first = b1 * input - a1 * filtered + filter.second;
            filter.second = b2 * input - a2 * filtered;
            double output = filtered * gain;
            // Leave ordinary samples linear; a smooth knee protects unusually
            // loud chords from integer wraparound or hard clipping after EQ.
            constexpr double knee = 24576, headroom = 32767 - knee;
            const double magnitude = std::abs(output);
            if (magnitude > knee) {
                output = std::copysign(
                    knee + headroom * (1 - std::exp(-(magnitude - knee) / headroom)), output);
            }
            sample = static_cast<int16_t>(std::lround(output));
        }
    }
}
} // namespace doomaudio
