// SPDX-License-Identifier: GPL-3.0-or-later
#include "snd_local.h"
#include <cstring>
namespace doomaudio {
namespace {
uint16_t U16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}
uint32_t U32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24);
}
} // namespace
bool DecodeWave(const uint8_t *data, size_t length, Wave &wave) {
    if (!data || length < 12 || std::memcmp(data, "RIFF", 4) || std::memcmp(data + 8, "WAVE", 4)) {
        return false;
    }
    const size_t end = size_t(U32(data + 4)) + 8;
    if (end > length || end < 12) {
        return false;
    }
    int channels = 0, rate = 0, bits = 0;
    const uint8_t *pcm = nullptr;
    size_t size = 0;
    for (size_t offset = 12; offset + 8 <= end;) {
        const auto *chunk = data + offset;
        const size_t bytes = U32(chunk + 4);
        offset += 8;
        if (bytes > end - offset) {
            return false;
        }
        if (!std::memcmp(chunk, "fmt ", 4)) {
            if (bytes < 16 || U16(data + offset) != 1) {
                return false;
            }
            channels = U16(data + offset + 2);
            rate = U32(data + offset + 4);
            bits = U16(data + offset + 14);
            if ((channels != 1 && channels != 2) || rate < 4000 || rate > 192000 ||
                (bits != 8 && bits != 16) || U16(data + offset + 12) != channels * bits / 8) {
                return false;
            }
        } else if (!std::memcmp(chunk, "data", 4)) {
            pcm = data + offset;
            size = bytes;
        }
        offset += bytes;
        // Several Doom exporters omit the final odd-byte RIFF pad.
        if ((bytes & 1) && offset < end) {
            ++offset;
        }
    }
    if (!pcm || !size || !channels || size % (channels * bits / 8) || size > 64 * 1024 * 1024) {
        return false;
    }
    Wave decoded;
    decoded.rate = rate;
    decoded.channels = channels;
    decoded.samples.resize(size / (bits / 8));
    for (size_t i = 0; i < decoded.samples.size(); ++i) {
        decoded.samples[i] =
            bits == 8 ? int16_t((int(pcm[i]) - 128) * 256) : int16_t(U16(pcm + i * 2));
    }
    wave = std::move(decoded);
    return true;
}
} // namespace doomaudio
