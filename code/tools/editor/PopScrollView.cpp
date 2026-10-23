#include "PopScrollView.h"
#include <algorithm>
#include <cmath>
namespace de {
void panZoom(App &a) {
    auto &io = ImGui::GetIO();
    if (ImGui::IsMouseDragging(2, 0)) {
        a.view.center.x -= io.MouseDelta.x / a.view.zoom;
        a.view.center.y += io.MouseDelta.y / a.view.zoom;
    }
    if (io.MouseWheel != 0) {
        auto before = screenToWorld(a, io.MousePos);
        a.view.zoom = std::clamp(a.view.zoom * std::pow(1.2f, io.MouseWheel), .02f, 32.f);
        auto after = screenToWorld(a, io.MousePos);
        a.view.center = a.view.center + before - after;
    }
}
} // namespace de
