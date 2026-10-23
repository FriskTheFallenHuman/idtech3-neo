#include "ThingWindow.h"
namespace de {
void drawThingWindow(App &a) {
    if (floatingPanel("Thing palette", &a.showThings, {450, 490}))
        drawThingPalette(a);
    ImGui::End();
}
} // namespace de
