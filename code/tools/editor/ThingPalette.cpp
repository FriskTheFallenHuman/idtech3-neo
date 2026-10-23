#include "ThingPalette.h"
#include "ModelProps.h"
#include "AssetImages.h"
#include "formats/sprite_pack.h"
#include "formats/sprite_profiles.h"
#include <algorithm>
namespace de {
void drawThingLabel(const App &a, ImDrawList *draw, int type, ImVec2 top, ImU32 color) {
    std::string name;
    for (const auto &definition : a.thingDefs)
        if (definition.type == type) {
            name = definition.name;
            break;
        }
    if (name.empty())
        name = "Thing " + std::to_string(type);
    for (auto &c : name)
        if (c == '_')
            c = ' ';
    const auto size = ImGui::CalcTextSize(name.c_str(), nullptr, false);
    ImVec2 pos{top.x - size.x * .5f, top.y - size.y - 4.f * a.dpi};
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            if (x || y)
                draw->AddText({pos.x + x * a.dpi, pos.y + y * a.dpi}, IM_COL32(0, 0, 0, 255),
                              name.c_str());
    draw->AddText(pos, color, name.c_str());
}
void initializeThingDefinitions(App &a) {
    if (a.thingDefs.empty()) {
        const std::pair<int, const char *> defs[] = {{1, "Player_1"},
                                                     {2, "Player_2"},
                                                     {3, "Player_3"},
                                                     {4, "Player_4"},
                                                     {11, "Deathmatch_start"},
                                                     {3004, "Former_human"},
                                                     {9, "Shotgun_guy"},
                                                     {3001, "Imp"},
                                                     {3002, "Demon"},
                                                     {58, "Spectre"},
                                                     {3005, "Cacodemon"},
                                                     {3003, "Baron"},
                                                     {3006, "Lost_soul"},
                                                     {16, "Cyberdemon"},
                                                     {7, "Spider_mastermind"},
                                                     {2001, "Shotgun"},
                                                     {2002, "Chaingun"},
                                                     {2003, "Rocket_launcher"},
                                                     {2004, "Plasma_rifle"},
                                                     {2005, "Chainsaw"},
                                                     {2006, "BFG_9000"},
                                                     {2007, "Ammo_clip"},
                                                     {2008, "Shells"},
                                                     {2011, "Stimpack"},
                                                     {2012, "Medikit"},
                                                     {2013, "Soul_sphere"},
                                                     {2018, "Green_armor"},
                                                     {2019, "Blue_armor"},
                                                     {5, "Blue_key"},
                                                     {6, "Yellow_key"},
                                                     {13, "Red_key"},
                                                     {2035, "Barrel"},
                                                     {64, "Arch-vile"},
                                                     {66, "Revenant"},
                                                     {67, "Mancubus"},
                                                     {65, "Heavy_weapon_dude"},
                                                     {69, "Hell_knight"},
                                                     {68, "Arachnotron"},
                                                     {71, "Pain_elemental"},
                                                     {84, "Wolfenstein_SS"},
                                                     {72, "Commander_Keen"},
                                                     {88, "Boss_brain"},
                                                     {89, "Boss_shooter"},
                                                     {87, "Boss_target"},
                                                     {14, "Teleport_destination"},
                                                     {2014, "Health_bonus"},
                                                     {2015, "Armor_bonus"},
                                                     {39, "Yellow_skull_key"},
                                                     {38, "Red_skull_key"},
                                                     {40, "Blue_skull_key"},
                                                     {2022, "Invulnerability"},
                                                     {2023, "Berserk"},
                                                     {2024, "Partial_invisibility"},
                                                     {2025, "Radiation_suit"},
                                                     {2026, "Computer_area_map"},
                                                     {2045, "Light_amplification"},
                                                     {83, "Megasphere"},
                                                     {2048, "Box_of_ammo"},
                                                     {2010, "Rocket"},
                                                     {2046, "Box_of_rockets"},
                                                     {2047, "Energy_cell"},
                                                     {17, "Energy_cell_pack"},
                                                     {2049, "Box_of_shells"},
                                                     {8, "Backpack"},
                                                     {82, "Super_shotgun"},
                                                     {85, "Tall_tech_lamp"},
                                                     {86, "Short_tech_lamp"},
                                                     {2028, "Floor_lamp"},
                                                     {30, "Tall_green_pillar"},
                                                     {31, "Short_green_pillar"},
                                                     {32, "Tall_red_pillar"},
                                                     {33, "Short_red_pillar"},
                                                     {37, "Red_pillar_with_skull"},
                                                     {36, "Green_pillar_with_heart"},
                                                     {41, "Evil_eye"},
                                                     {42, "Floating_skull"},
                                                     {43, "Burnt_tree"},
                                                     {44, "Tall_blue_torch"},
                                                     {45, "Tall_green_torch"},
                                                     {46, "Tall_red_torch"},
                                                     {55, "Short_blue_torch"},
                                                     {56, "Short_green_torch"},
                                                     {57, "Short_red_torch"},
                                                     {47, "Stalagmite"},
                                                     {48, "Tech_pillar"},
                                                     {34, "Candle"},
                                                     {35, "Candelabra"},
                                                     {49, "Hanging_twitching_victim"},
                                                     {50, "Hanging_victim_arms_out"},
                                                     {51, "Hanging_one-legged_victim"},
                                                     {52, "Hanging_pair_of_legs"},
                                                     {53, "Hanging_leg"},
                                                     {59, "Hanging_pair_of_legs_nonblocking"},
                                                     {60, "Hanging_victim_arms_out_nonblocking"},
                                                     {61, "Hanging_one-legged_victim_nonblocking"},
                                                     {62, "Hanging_leg_nonblocking"},
                                                     {63, "Hanging_twitching_victim_nonblocking"},
                                                     {22, "Dead_cacodemon"},
                                                     {15, "Dead_player"},
                                                     {18, "Dead_former_human"},
                                                     {21, "Dead_demon"},
                                                     {23, "Dead_lost_soul"},
                                                     {20, "Dead_imp"},
                                                     {19, "Dead_shotgun_guy"},
                                                     {10, "Gibs"},
                                                     {12, "Gibs_nonblocking"},
                                                     {28, "Heads_on_a_pole"},
                                                     {24, "Pool_of_blood_and_flesh"},
                                                     {27, "Skull_on_a_pole"},
                                                     {29, "Heads_and_candles"},
                                                     {25, "Impaled_human"},
                                                     {26, "Twitching_impaled_human"},
                                                     {54, "Large_brown_tree"},
                                                     {70, "Burning_barrel"},
                                                     {73, "Hanging_gutless_victim"},
                                                     {74, "Hanging_victim_no_brain"},
                                                     {75, "Hanging_torso_looking_down"},
                                                     {76, "Hanging_torso_open_skull"},
                                                     {77, "Hanging_torso_looking_up"},
                                                     {78, "Hanging_torso_no_brain"},
                                                     {79, "Pool_of_blood"},
                                                     {80, "Pool_of_blood_nonblocking"},
                                                     {81, "Pool_of_brains"}};
        for (auto [type, name] : defs)
            a.thingDefs.push_back({name, "-", 0, type, 7});
    }
    // Register custom enemies even when a project loads its own things.dsp.
    bool hasEggZombie = false;
    bool hasEggBoss = false;
    static const bool eggElemental = [] {
        const auto frames = doom_sprites::discover(sprite_baker::packsDirectory());
        for (const auto &entry : frames)
            if (entry.second.directory.filename() == L"EggElemental")
                return true;
        return false;
    }();
    for (auto &definition : a.thingDefs) {
        if (definition.type == 71)
            hasEggBoss = true;
        if (definition.type == 71 && eggElemental)
            definition.name = "Egg_Elemental_(Pain_elemental)";
        if (definition.type == 32001)
            hasEggZombie = true;
    }
    if (!hasEggBoss && eggElemental)
        a.thingDefs.push_back({"Egg_Elemental_(Pain_elemental)", "PAINA", 0, 71, 7});
    if (!hasEggZombie)
        a.thingDefs.push_back({"Egg_Zombie", "SSWVA", 0, 32001, 7});
    for (const auto &[type, model] : modelProps().definitions) {
        bool present = false;
        for (const auto &definition : a.thingDefs)
            if (definition.type == type)
                present = true;
        if (!present) {
            auto name = model.name;
            for (auto &c : name)
                if (c == ' ' || c == '\t')
                    c = '_'; // things.dsp uses single-token names.
            a.thingDefs.push_back({name, "-", 0, type, 7, {.9f, .65f, .2f, 1}});
        }
    }
}
void drawThingPalette(App &a) {
    ImGui::TextDisabled("OBJECT TO PLACE");
    ImGui::TextWrapped("%s", placementObjectName(a).c_str());
    ImGui::TextDisabled("Type %d", a.thingType);
    ImGui::TextWrapped("Choose an object below, then click in Plan to place it.");
    if (ImGui::Button(a.view.tool == 3 ? "Placing in Plan  |  Esc to stop" : "Place in Plan",
                      {-1, 0})) {
        a.requestObjectPlacement = true;
    }
    if (ImGui::CollapsingHeader("Placement settings")) {
        ImGui::InputInt("Type ID", &a.thingType);
        a.thingType = std::clamp(a.thingType, 1, 65535);
        ImGui::InputInt("Angle", &a.thingAngle);
        ImGui::InputInt("Spawn flags", &a.thingOptions);
    }
    ImGui::Separator();
    drawThingNames(a);
}
} // namespace de
