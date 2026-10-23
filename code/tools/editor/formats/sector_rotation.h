#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

// DSURFROT v1: LE version/count, then floor/ceiling millidegrees per sector.
struct SectorRotation {
    int32_t floor = 0, ceiling = 0;
    bool operator==(const SectorRotation &b) const {
        return floor == b.floor && ceiling == b.ceiling;
    }
    bool active() const { return floor != 0 || ceiling != 0; }
};
inline int32_t textureMillidegrees(float degrees) {
    if (!std::isfinite(degrees))
        return 0;
    return int32_t(std::lround(std::remainder(double(degrees), 360.0) * 1000.0));
}
inline std::array<float, 2> rotatedPlaneUV(float x, float y, int32_t angle) {
    const double radians = angle * (3.141592653589793 / 180000.0);
    const float c = float(std::cos(radians)), s = float(std::sin(radians));
    return {c * x + s * y, s * x - c * y};
}
inline std::vector<uint8_t> encodeRotations(const std::vector<SectorRotation> &planes) {
    bool active = false;
    for (const auto &p : planes)
        active |= p.active();
    if (!active)
        return {};
    std::vector<uint8_t> bytes;
    auto put = [&](uint32_t n) {
        for (int i = 0; i < 4; ++i)
            bytes.push_back(uint8_t(n >> (8 * i)));
    };
    put(1);
    put(uint32_t(planes.size()));
    for (const auto &p : planes) {
        put(p.floor);
        put(p.ceiling);
    }
    return bytes;
}
inline std::vector<SectorRotation> decodeRotations(const uint8_t *bytes, size_t size,
                                                   size_t count) {
    auto get = [&](size_t p) {
        return uint32_t(bytes[p]) | uint32_t(bytes[p + 1]) << 8 | uint32_t(bytes[p + 2]) << 16 |
               uint32_t(bytes[p + 3]) << 24;
    };
    if (size < 8 || get(0) != 1 || get(4) != count || size != 8 + count * 8)
        throw std::runtime_error("Invalid DSURFROT version, sector count or size");
    std::vector<SectorRotation> planes(count);
    for (size_t i = 0; i < count; ++i) {
        auto &p = planes[i];
        p = {int32_t(get(8 + i * 8)), int32_t(get(12 + i * 8))};
        if (p.floor < -360000 || p.floor > 360000 || p.ceiling < -360000 || p.ceiling > 360000)
            throw std::runtime_error("Texture rotation exceeds supported bounds");
    }
    return planes;
}
