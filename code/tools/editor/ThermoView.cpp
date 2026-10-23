#include "ThermoView.h"
#include <algorithm>
namespace de {
void drawProgress(float value, const char *label) {
    ImGui::ProgressBar(std::clamp(value, 0.f, 1.f), {-1, 0}, label);
}
} // namespace de
