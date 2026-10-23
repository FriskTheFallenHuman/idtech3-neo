#include "TextureView.h"
#include <algorithm>
namespace de {
void drawTextureView(App &a, const Texture &t, float scale) {
    ImTextureID id = a.image("texture:" + t.name, [&] { return a.project.wad.texture(t); });
    if (id) {
        float fit = std::min(scale, ImGui::GetContentRegionAvail().x / std::max(1, t.width));
        ImGui::Image(id, {t.width * fit, t.height * fit});
    } else
        ImGui::TextDisabled("Preview unavailable; resource patch is missing.");
}
} // namespace de
