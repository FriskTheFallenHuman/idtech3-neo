// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace doomaudio {
struct Wave {
    int rate = 0, channels = 0;
    std::vector<int16_t> samples;
};
bool DecodeWave(const uint8_t *data, size_t length, Wave &wave);
void MusicInit();
void MusicShutdown();
void MusicUpdate(float volume);
void MusicStart(const char *intro, const char *loop);
void MusicStop();
void MusicInfo();
unsigned NewStreamSource();
void QueuePCM(unsigned source, const void *data, int bytes, int rate, int channels, int width);
void ClearStream(unsigned source);
} // namespace doomaudio
