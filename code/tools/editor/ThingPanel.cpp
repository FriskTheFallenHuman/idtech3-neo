#include "ThingPanel.h"
#include "idfunctions.h"
#include <sstream>
namespace de {
void drawThingPanel(App &a) {
    int first = -1, count = 0;
    for (size_t i = 0; i < a.world.things.size(); ++i)
        if (a.world.things[i].selected > 0) {
            if (first < 0)
                first = int(i);
            ++count;
        }
    if (first < 0) {
        ImGui::TextWrapped("Select a thing or use Place thing.");
        ImGui::InputInt("New thing type", &a.thingType);
        return;
    }
    static Thing edit;
    static int prev = -1;
    static std::string signature;
    auto sig = a.world.serialize();
    if (prev != first || signature != sig) {
        edit = a.world.things[first];
        prev = first;
        signature = sig;
    }
    ImGui::Text("%d selected | Thing %d", count, first);
    ImGui::InputInt("Type", &edit.type);
    ImGui::InputInt("Angle", &edit.angle);
    ImGui::InputInt("Options", &edit.options);
    ImGui::InputInt("Height offset", &edit.height);
    for (int bit = 0; bit < 5; ++bit) {
        const char *labels[] = {"Easy", "Medium", "Hard", "Ambush / deaf", "Multiplayer only"};
        bool flag = (edit.options & (1 << bit)) != 0;
        if (ImGui::Checkbox(labels[bit], &flag)) {
            if (flag)
                edit.options |= 1 << bit;
            else
                edit.options &= ~(1 << bit);
        }
    }
    if (ImGui::Button("Apply thing properties")) {
        a.world.checkpoint();
        for (auto &t : a.world.things)
            if (t.selected > 0) {
                t.type = edit.type;
                t.angle = ((edit.angle % 360) + 360) % 360;
                t.options = edit.options;
                t.height = std::clamp(edit.height, -32768, 32767);
            }
    }
}
void loadThingDefinitions(App &a, const std::filesystem::path &path) {
    auto bytes = readFile(path);
    std::istringstream in(std::string(bytes.begin(), bytes.end()));
    std::string row;
    std::getline(in, row);
    int n = 0;
    require(sscanf(row.c_str(), "numthings: %d", &n) == 1 && n >= 0 && n <= 65536,
            "Invalid thing definition count");
    std::vector<ThingDef> defs;
    for (int i = 0; i < n; ++i) {
        require(bool(std::getline(in, row)), "Truncated things.dsp");
        ThingDef t;
        char name[256]{}, sprite[256]{};
        require(sscanf(row.c_str(), "%255s = %d %d %d (%f %f %f) %255s", name, &t.angle, &t.type,
                       &t.options, &t.color.x, &t.color.y, &t.color.z, sprite) == 8,
                "Malformed thing definition");
        t.name = name;
        t.sprite = sprite;
        defs.push_back(t);
    }
    a.thingDefs = std::move(defs);
}
void saveThingDefinitions(App &a, const std::filesystem::path &path) {
    std::ostringstream out;
    out << "numthings: " << a.thingDefs.size() << '\n';
    for (auto &t : a.thingDefs)
        out << t.name << " = " << t.angle << ' ' << t.type << ' ' << t.options << " (" << t.color.x
            << ' ' << t.color.y << ' ' << t.color.z << ") " << (t.sprite.empty() ? "-" : t.sprite)
            << '\n';
    writeTextAtomic(path, out.str());
}
} // namespace de
