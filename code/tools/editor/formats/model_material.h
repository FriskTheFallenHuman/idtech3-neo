#pragma once
#include "md3_model.h"
#include "material_manifest.h"

namespace doom_models {
// Randomize the order within pairs, guaranteeing a mix when several cars spawn.
inline unsigned carFlashPhase(unsigned seed, unsigned ordinal) {
    unsigned hash = seed + (ordinal / 2) * 0x9e3779b9u;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return ((hash ^ (ordinal & 1u)) & 1u) * 10u;
}
inline doom_materials::Emission skinEmission(const std::filesystem::path &skin) {
    const auto settings = std::filesystem::path(skin.u8string() + ".material");
    if (!std::filesystem::exists(settings))
        return {};
    const auto bytes = read(settings);
    std::istringstream fields(std::string(bytes.begin(), bytes.end()));
    std::string option;
    require(bool(fields >> option) && option == "EMISSIVE",
            "Expected EMISSIVE in " + settings.u8string());
    return doom_materials::parseEmission(fields);
}
inline Vec3 pointAttachment(const doom_materials::Emission &e, float scale, float angle,
                            Vec3 origin) {
    auto local = rotate({e.pointX * scale, e.pointY * scale, e.pointZ * scale}, angle);
    return {origin.x + local.x, origin.y + local.y, origin.z + local.z};
}
} // namespace doom_models
