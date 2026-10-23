#pragma once
#include "formats/texture_uv.h"
#include "imgui.h"
#include <algorithm>
namespace de {
inline bool editTextureUV(TextureUV &uv) {
    bool changed = false;
    float scale[2] = {uv.scaleU * .001f, uv.scaleV * .001f};
    float offset[2] = {uv.offsetU * .001f, uv.offsetV * .001f};
    if (ImGui::InputFloat2("Scale U / V", scale, "%.3f")) {
        if (std::isfinite(scale[0]) && std::isfinite(scale[1])) {
            uv.scaleU = int(std::lround(std::clamp(scale[0], .001f, 1000.f) * 1000));
            uv.scaleV = int(std::lround(std::clamp(scale[1], .001f, 1000.f) * 1000));
            changed = true;
        }
    }
    if (ImGui::InputFloat2("Offset U / V", offset, "%.3f")) {
        if (std::isfinite(offset[0]) && std::isfinite(offset[1])) {
            uv.offsetU = int(std::lround(std::clamp(offset[0], -32768.f, 32767.f) * 1000));
            uv.offsetV = int(std::lround(std::clamp(offset[1], -32768.f, 32767.f) * 1000));
            changed = true;
        }
    }
    ImGui::TextUnformatted(
        "Changes apply as you edit. Scale 2 = twice as large.\nOffsets use logical texture units.");
    if (ImGui::Button("Reset UVs")) {
        uv = {};
        changed = true;
    }
    return changed;
}
} // namespace de
