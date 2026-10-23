#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

// DSLOPES v1: LE version/count, then x/y (map units), dx/dy (16.16 rise/run).
// The sector floorheight is the height at the anchor. Missing lump means flat.
struct FloorSlope {
    int32_t x = 0, y = 0, dx = 0, dy = 0;
    bool active() const { return dx != 0 || dy != 0; }
    bool operator==(const FloorSlope &b) const {
        return x == b.x && y == b.y && dx == b.dx && dy == b.dy;
    }
    double offset(double px, double py) const { return ((px - x) * dx + (py - y) * dy) / 65536.0; }
};
inline std::vector<uint8_t> encodeSlopes(const std::vector<FloorSlope> &slopes) {
    std::vector<uint8_t> bytes;
    bool active = false;
    for (const auto &s : slopes)
        active |= s.active();
    if (!active)
        return bytes;
    auto put = [&](uint32_t n) {
        for (int i = 0; i < 4; ++i)
            bytes.push_back(uint8_t(n >> (8 * i)));
    };
    put(1);
    put(uint32_t(slopes.size()));
    for (const auto &s : slopes) {
        put(s.x);
        put(s.y);
        put(s.dx);
        put(s.dy);
    }
    return bytes;
}
inline std::vector<FloorSlope> decodeSlopes(const uint8_t *bytes, size_t size, size_t count) {
    auto get = [&](size_t p) {
        return uint32_t(bytes[p]) | uint32_t(bytes[p + 1]) << 8 | uint32_t(bytes[p + 2]) << 16 |
               uint32_t(bytes[p + 3]) << 24;
    };
    if (size < 8 || get(0) != 1 || get(4) != count || size != 8 + count * 16)
        throw std::runtime_error("Invalid DSLOPES version, sector count or size");
    std::vector<FloorSlope> slopes(count);
    for (size_t i = 0; i < count; ++i) {
        size_t p = 8 + i * 16;
        slopes[i] = {int32_t(get(p)), int32_t(get(p + 4)), int32_t(get(p + 8)),
                     int32_t(get(p + 12))};
        const auto &s = slopes[i];
        if (s.x < -32768 || s.x > 32767 || s.y < -32768 || s.y > 32767 || s.dx < -16 * 65536 ||
            s.dx > 16 * 65536 || s.dy < -16 * 65536 || s.dy > 16 * 65536)
            throw std::runtime_error("DSLOPES plane exceeds supported bounds");
    }
    return slopes;
}
