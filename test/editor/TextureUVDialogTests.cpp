#include "TextureUVEditor.h"
#include <cstdio>
int main() {
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    unsigned char *pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    // Exercise the actual InputFloat2 widgets with assertions enabled. The old
    // EnterReturnsTrue flag crashed on opening either wall or sector UV panels.
    TextureUV uv;
    for (int frame = 0; frame < 3; ++frame) {
        ImGui::NewFrame();
        ImGui::Begin("Wall texture UVs");
        if (de::editTextureUV(uv))
            return 1;
        ImGui::End();
        ImGui::Begin("Floor / ceiling texture UVs");
        if (de::editTextureUV(uv))
            return 2;
        ImGui::End();
        ImGui::Render();
    }
    ImGui::DestroyContext();
    if (!(uv == TextureUV{}))
        return 3;
    std::puts("Wall and sector UV dialogs opened without assertion");
    return 0;
}
