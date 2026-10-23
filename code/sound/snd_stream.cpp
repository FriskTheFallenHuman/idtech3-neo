// SPDX-License-Identifier: GPL-3.0-or-later
#include "snd_local.h"
#include "../openal/include/AL/al.h"
namespace doomaudio {
unsigned NewStreamSource() {
    ALuint source = 0;
    alGenSources(1, &source);
    alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(source, AL_ROLLOFF_FACTOR, 0);
    return source;
}
void ClearStream(unsigned source) {
    if (!source) {
        return;
    }
    alSourceStop(source);
    ALint count = 0;
    alGetSourcei(source, AL_BUFFERS_QUEUED, &count);
    while (count-- > 0) {
        ALuint buffer;
        alSourceUnqueueBuffers(source, 1, &buffer);
        alDeleteBuffers(1, &buffer);
    }
}
void QueuePCM(unsigned source, const void *data, int bytes, int rate, int channels, int width) {
    if (!source || bytes <= 0) {
        return;
    }
    ALint processed = 0;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint old;
        alSourceUnqueueBuffers(source, 1, &old);
        alDeleteBuffers(1, &old);
    }
    ALuint buffer;
    alGenBuffers(1, &buffer);
    const ALenum format = channels == 1 ? (width == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16)
                                        : (width == 1 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16);
    alBufferData(buffer, format, data, bytes, rate);
    alSourceQueueBuffers(source, 1, &buffer);
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
        alSourcePlay(source);
    }
}
} // namespace doomaudio
