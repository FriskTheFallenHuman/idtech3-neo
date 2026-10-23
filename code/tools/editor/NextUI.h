#pragma once
#include "UI.h"
#include <array>
namespace de {
struct NextControl {
    std::string kind, text, font, image, action, outlet;
    std::array<float, 4> rect;
    float fontsize;
    int row, flags;
};
struct NextPanel {
    std::string key, title;
    std::array<float, 4> frame;
    std::vector<NextControl> controls;
};
void testNextBindings();
void applyDesktopStyle(float scale);
Image nextBitmap(const std::string &name);
int nativeContextMenu(const std::vector<std::string> &labels, ImVec2 clientPosition);
void showNativePanel(const std::string &key);
void closeNativePanel();
void minimizeNativePanel();
void renderNextWindow(App &, const std::string &key, const std::string &title);
ImFont *nextFont(bool bold, float size);
ImFont *nextFont(const std::string &name, float size);
} // namespace de
