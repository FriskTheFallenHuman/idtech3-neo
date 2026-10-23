#include "FloatingPanel.h"
#include <algorithm>
namespace de {
bool floatingPanel(const char *title, bool *open, ImVec2 size) {
    float scale = ImGui::GetFontSize() / 16.f;
    auto viewport = ImGui::GetMainViewport();
    ImVec2 maximum{std::max(200.f, viewport->WorkSize.x - 40),
                   std::max(160.f, viewport->WorkSize.y - 80)};
    ImGui::SetNextWindowSizeConstraints(
        {std::min(240 * scale, maximum.x), std::min(140 * scale, maximum.y)}, maximum);
    ImGui::SetNextWindowSize(
        {std::min(size.x * scale, maximum.x), std::min(size.y * scale, maximum.y)},
        ImGuiCond_FirstUseEver);
    return ImGui::Begin(title, open);
}
} // namespace de
