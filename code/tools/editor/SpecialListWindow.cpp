#include "SpecialListWindow.h"
namespace de {
void defaultSpecials(App &a) {
    a.lineSpecials.entries = {
        {0, "Normal"},         {1, "Door open / close"},    {2, "W1 door open"},
        {3, "W1 door close"},  {5, "W1 raise floor"},       {11, "S1 exit"},
        {26, "Blue key door"}, {27, "Yellow key door"},     {28, "Red key door"},
        {39, "W1 teleport"},   {48, "Scroll texture left"}, {51, "S1 secret exit"},
        {97, "WR teleport"}};
    a.sectorSpecials.entries = {{0, "Normal"},      {1, "Blink random"},      {2, "Blink 0.5 sec"},
                                {3, "Blink 1 sec"}, {4, "Damage and blink"},  {5, "Damage 10%"},
                                {7, "Damage 5%"},   {8, "Oscillating light"}, {9, "Secret"},
                                {16, "Damage 20%"}};
}
void drawSpecials(App &a) {
    if (floatingPanel("Specials", &a.showSpecials, {430, 450})) {
        static bool sectors = false;
        ImGui::Checkbox("Sector specials", &sectors);
        auto &list = sectors ? a.sectorSpecials : a.lineSpecials;
        for (auto &s : list.entries) {
            auto label = std::to_string(s.value) + ": " + s.description;
            if (ImGui::Selectable(label.c_str())) {
                a.world.checkpoint();
                for (auto &l : a.world.lines)
                    if (l.selected > 0) {
                        if (sectors)
                            l.side[l.selected - 1].ends.special = s.value;
                        else
                            l.special = s.value;
                    }
            }
        }
        ImGui::Separator();
        static int value = 0;
        static char description[256]{};
        ImGui::InputInt("Value", &value);
        ImGui::InputText("Description", description, sizeof(description));
        if (ImGui::Button("Add / update")) {
            bool found = false;
            for (auto &s : list.entries)
                if (s.value == value) {
                    s.description = description;
                    found = true;
                }
            if (!found)
                list.entries.push_back({value, description});
            a.projectDirty = true;
        }
    }
    ImGui::End();
}
} // namespace de
