#include "TextLog.h"
namespace de {
void drawLog(App &a) {
    if (floatingPanel("Log", &a.showLog, {740, 360})) {
        if (ImGui::Button("Clear"))
            a.log.clear();
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            std::string text;
            for (auto &s : a.log)
                text += s + '\n';
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::Separator();
        for (auto &s : a.log)
            ImGui::TextUnformatted(s.c_str());
    }
    ImGui::End();
}
} // namespace de
