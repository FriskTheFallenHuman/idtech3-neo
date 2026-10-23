#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>
// Fixed thousandths keep editor equality and text/binary round trips exact.
struct TextureUV {
    int32_t scaleU = 1000, scaleV = 1000, offsetU = 0, offsetV = 0;
    bool operator==(const TextureUV &b) const {
        return scaleU == b.scaleU && scaleV == b.scaleV && offsetU == b.offsetU &&
               offsetV == b.offsetV;
    }
    bool active() const { return !(*this == TextureUV{}); }
    void validate() const {
        if (scaleU < 1 || scaleU > 1000000 || scaleV < 1 || scaleV > 1000000 ||
            offsetU < -32768000 || offsetU > 32767000 || offsetV < -32768000 || offsetV > 32767000)
            throw std::runtime_error("Texture UV scale must be 0.001..1000; offset -32768..32767");
    }
    std::array<float, 2> apply(float u, float v) const {
        return {u * 1000.f / scaleU + offsetU * .001f, v * 1000.f / scaleV + offsetV * .001f};
    }
};
inline std::vector<uint8_t> encodeTextureUVs(const std::vector<TextureUV> &uvs) {
    bool active = false;
    for (const auto &uv : uvs) {
        uv.validate();
        active |= uv.active();
    }
    if (!active)
        return {};
    std::vector<uint8_t> out;
    auto put = [&](uint32_t n) {
        for (int i = 0; i < 4; ++i)
            out.push_back(uint8_t(n >> (8 * i)));
    };
    put(1);
    put(uint32_t(uvs.size()));
    for (const auto &uv : uvs) {
        put(uv.scaleU);
        put(uv.scaleV);
        put(uv.offsetU);
        put(uv.offsetV);
    }
    return out;
}
inline std::vector<TextureUV> decodeTextureUVs(const uint8_t *data, size_t bytes, size_t count) {
    auto get = [&](size_t i) {
        return uint32_t(data[i]) | uint32_t(data[i + 1]) << 8 | uint32_t(data[i + 2]) << 16 |
               uint32_t(data[i + 3]) << 24;
    };
    if (bytes < 8 || get(0) != 1 || get(4) != count || bytes != 8 + 16 * count)
        throw std::runtime_error("Invalid texture UV lump version/count/size");
    std::vector<TextureUV> result(count);
    for (size_t i = 0; i < count; ++i) {
        auto &uv = result[i];
        size_t p = 8 + i * 16;
        uv = {int32_t(get(p)), int32_t(get(p + 4)), int32_t(get(p + 8)), int32_t(get(p + 12))};
        uv.validate();
    }
    return result;
}
