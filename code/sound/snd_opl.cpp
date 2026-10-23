// SPDX-License-Identifier: GPL-3.0-or-later
#include "snd_opl.h"
#include "opl/doom_opl.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace doomaudio {
namespace {
constexpr uint64_t Rate = 44100;
struct Event {
    uint64_t tick = 0, sample = 0;
    uint32_t tempo = 0;
    uint8_t status = 0, first = 0, second = 0;
};
std::vector<Event> events;
size_t nextEvent;
uint64_t position, duration;
bool loaded;
const char *error = "";
uint16_t Big16(const uint8_t *p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
uint32_t Big32(const uint8_t *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
struct Reader {
    const uint8_t *data;
    size_t length, offset = 0;
    bool Byte(uint8_t &value) {
        if (offset >= length) {
            return false;
        }
        value = data[offset++];
        return true;
    }
    bool Variable(uint32_t &value) {
        value = 0;
        for (int i = 0; i < 4; ++i) {
            uint8_t part;
            if (!Byte(part)) {
                return false;
            }
            value = (value << 7) | (part & 127);
            if (!(part & 128)) {
                return true;
            }
        }
        return false;
    }
};
bool Track(const uint8_t *data, size_t length, std::vector<Event> &output, uint64_t &endTick) {
    Reader input{data, length};
    uint64_t tick = 0;
    uint8_t running = 0;
    while (input.offset < length) {
        uint32_t delta;
        uint8_t status;
        if (!input.Variable(delta) || !input.Byte(status)) {
            return false;
        }
        tick += delta;
        if (tick > uint64_t(1) << 40) {
            return false;
        }
        if (status < 128) {
            if (!running) {
                return false;
            }
            --input.offset;
            status = running;
        }
        if (status == 0xff || status == 0xf0 || status == 0xf7) {
            uint8_t type = 0;
            uint32_t count;
            if ((status == 0xff && !input.Byte(type)) || !input.Variable(count) ||
                count > length - input.offset) {
                return false;
            }
            if (status == 0xff && type == 0x51) {
                if (count != 3) {
                    return false;
                }
                const auto *p = data + input.offset;
                const uint32_t tempo = (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2];
                if (!tempo) {
                    return false;
                }
                output.push_back({tick, 0, tempo});
            }
            input.offset += count;
            if (status == 0xff && type == 0x2f) {
                if (count) {
                    return false;
                }
                endTick = (std::max)(endTick, tick);
                return true;
            }
            if (status != 0xff) {
                running = 0;
            }
        } else {
            if (status < 0x80 || status >= 0xf0) {
                return false;
            }
            running = status;
            uint8_t first, second = 0;
            if (!input.Byte(first) || first > 127) {
                return false;
            }
            if ((status & 0xe0) != 0xc0 && (!input.Byte(second) || second > 127)) {
                return false;
            }
            output.push_back({tick, 0, 0, status, first, second});
        }
        if (output.size() > 1000000) {
            return false;
        }
    }
    return false; // A track must have a bounded end marker.
}
} // namespace
bool OPLLoad(const uint8_t *midi, size_t midiLength, const uint8_t *instruments,
             size_t instrumentLength) {
    OPLStop();
    error = "invalid MIDI header";
    if (!midi || midiLength < 14 || midiLength > 32 * 1024 * 1024 || std::memcmp(midi, "MThd", 4)) {
        return false;
    }
    const size_t header = Big32(midi + 4);
    if (header < 6 || header > midiLength - 8) {
        return false;
    }
    const int format = Big16(midi + 8), tracks = Big16(midi + 10), division = Big16(midi + 12);
    if (format > 1 || tracks < 1 || tracks > 256 || (format == 0 && tracks != 1) || !division ||
        (division & 0x8000)) {
        return false;
    }
    std::vector<Event> parsed;
    size_t offset = 8 + header;
    uint64_t endTick = 0;
    for (int track = 0; track < tracks; ++track) {
        error = "invalid MIDI track bounds or event";
        if (offset > midiLength || midiLength - offset < 8 ||
            std::memcmp(midi + offset, "MTrk", 4)) {
            return false;
        }
        const size_t length = Big32(midi + offset + 4);
        offset += 8;
        if (length > midiLength - offset || !Track(midi + offset, length, parsed, endTick)) {
            return false;
        }
        offset += length;
    }
    std::stable_sort(parsed.begin(), parsed.end(),
                     [](const Event &a, const Event &b) { return a.tick < b.tick; });
    uint64_t tick = 0, sample = 0, remainder = 0;
    uint32_t tempo = 500000;
    const uint64_t denominator = uint64_t(division) * 1000000;
    parsed.push_back({endTick});
    for (auto &event : parsed) {
        const uint64_t delta = event.tick - tick;
        if (delta > (std::numeric_limits<uint64_t>::max() - remainder) / (uint64_t(tempo) * Rate)) {
            error = "MIDI time overflow";
            return false;
        }
        const uint64_t numerator = delta * tempo * Rate + remainder;
        sample += numerator / denominator;
        remainder = numerator % denominator;
        if (sample > Rate * 60 * 60 * 4) {
            error = "MIDI duration exceeds four hours";
            return false;
        }
        event.sample = sample;
        tick = event.tick;
        if (event.tempo) {
            tempo = event.tempo;
        }
    }
    if (!sample) {
        error = "empty MIDI score";
        return false;
    }
    error = "invalid GENMIDI instrument table";
    if (!DoomOPL_Reset(instruments, instrumentLength)) {
        return false;
    }
    events = std::move(parsed);
    position = nextEvent = 0;
    duration = sample;
    loaded = true;
    error = "";
    return true;
}
void OPLStop() {
    loaded = false;
    events.clear();
    position = duration = nextEvent = 0;
}
bool OPLLoaded() {
    return loaded;
}
size_t OPLRender(int16_t *stereo, size_t frames) {
    if (!loaded || !stereo) {
        return 0;
    }
    size_t rendered = 0;
    while (rendered < frames && position < duration) {
        while (nextEvent < events.size() && events[nextEvent].sample <= position) {
            const auto &event = events[nextEvent++];
            if (event.status) {
                DoomOPL_Event(event.status, event.first, event.second);
            }
        }
        uint64_t end = duration;
        if (nextEvent < events.size()) {
            end = (std::min)(end, events[nextEvent].sample);
        }
        const size_t count =
            static_cast<size_t>((std::min)(uint64_t(frames - rendered), end - position));
        if (!count) {
            break;
        }
        DoomOPL_Render(stereo + rendered * 2, count);
        position += count;
        rendered += count;
    }
    return rendered;
}
unsigned OPLPosition() {
    return static_cast<unsigned>(position * 1000 / Rate);
}
unsigned OPLDuration() {
    return static_cast<unsigned>(duration * 1000 / Rate);
}
const char *OPLError() {
    return error;
}
} // namespace doomaudio
