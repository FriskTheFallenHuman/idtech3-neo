#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

// DTHINGZ v1: LE version/count, followed by signed map-unit Z offsets in
// THINGS order. The classic ten-byte THINGS record remains unchanged.
inline std::vector<uint8_t> encodeThingHeights(const std::vector<int32_t> &heights) {
    std::vector<uint8_t> data;
    if (std::all_of(heights.begin(), heights.end(), [](int32_t z) { return z == 0; }))
        return data;
    auto put = [&](uint32_t value) {
        for (int i = 0; i < 4; ++i)
            data.push_back(uint8_t(value >> (i * 8)));
    };
    put(1);
    put(uint32_t(heights.size()));
    for (auto z : heights) {
        if (z < -32768 || z > 32767)
            throw std::runtime_error("Thing height exceeds map bounds");
        put(uint32_t(z));
    }
    return data;
}
inline std::vector<int32_t> decodeThingHeights(const uint8_t *data, size_t size, size_t count) {
    auto get = [&](size_t at) {
        return uint32_t(data[at]) | uint32_t(data[at + 1]) << 8 | uint32_t(data[at + 2]) << 16 |
               uint32_t(data[at + 3]) << 24;
    };
    if (count > 1000000 || size < 8 || get(0) != 1 || get(4) != count || size != 8 + count * 4)
        throw std::runtime_error("Invalid DTHINGZ version, thing count or size");
    std::vector<int32_t> heights(count);
    for (size_t i = 0; i < count; ++i) {
        heights[i] = int32_t(get(8 + i * 4));
        if (heights[i] < -32768 || heights[i] > 32767)
            throw std::runtime_error("Thing height exceeds map bounds");
    }
    return heights;
}
