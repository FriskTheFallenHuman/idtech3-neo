#include "../formats/md3_model.h"
#include "../formats/model_material.h"
#include "../formats/model_collision.h"
#include "EditWorld.h"
#include "idfunctions.h"
#include <iostream>
using namespace doom_models;
int main(int argc, char **argv) {
    try {
        require(argc == 3, "expected root and output directory");
        const std::filesystem::path root = argv[1], out = argv[2];
        std::filesystem::create_directories(out);
        Registry registry;
        registry.discover(root / "assets/models");
        const auto *d = registry.find(15000);
        require(d != nullptr, "streetlight registered");
        const auto bytes = read(d->file);
        const auto model = decode(bytes);
        require(model.surfaces.size() == 2 && model.frames == 1, "streetlight surfaces");
        require(model.surfaces[1].fullbright(), "lens marked fullbright");
        registry.load(15000);
        const auto *signal = registry.find(15006);
        require(signal != nullptr, "traffic signal registered");
        const auto &traffic = registry.load(15006);
        require(signal->height == d->height && signal->height == 208,
                "traffic signal matches lamp post height");
        unsigned signalLights = 0;
        float armEnd = 0;
        std::vector<doom_materials::Emission> signalEmitters;
        for (const auto &surface : traffic.surfaces) {
            require(
                std::filesystem::exists(relativeAsset(signal->file.parent_path(), surface.shader)),
                "signal skin exists");
            for (const auto &v : surface.frames[0]) {
                require(v.position.z >= 0 && v.position.z <= 208, "signal bounds");
                armEnd = (std::max)(armEnd, v.position.x);
            }
            const auto e = skinEmission(relativeAsset(signal->file.parent_path(), surface.shader));
            if (e.strength <= 0) {
                require(!surface.fullbright(), "amber and green remain dark");
                continue;
            }
            ++signalLights;
            signalEmitters.push_back(e);
            require(surface.fullbright() && e.point && e.blink && e.strobePeriod == 35,
                    "red lens and attached light blink");
            require(e.red == 1 && e.green < .03 && e.blue < .03, "signal only emits red");
            for (unsigned tick = 0; tick < 350; ++tick)
                require(doom_materials::emissionBrightness(e, tick) == (tick % 35 < 18 ? 1.f : 0.f),
                        "single one-second traffic blink");
            const auto p = pointAttachment(e, 1, 3.14159265f / 2, {100, 200, 0});
            require(std::abs(p.x - (100 - e.pointY)) < .001 &&
                        std::abs(p.y - (200 + e.pointX)) < .001 && std::abs(p.z - e.pointZ) < .001,
                    "signal light rotates with housing");
        }
        require(signalLights == 3,
                "two overhead red signals and one pole red signal have illumination");
        require(armEnd == 272, "traffic arm extended by 1.7 times");
        require(signalEmitters[0].pointX == 136 && signalEmitters[1].pointX == 256 &&
                    signalEmitters[1].pointX - signalEmitters[0].pointX == 120,
                "overhead heads evenly cover lanes");
        require(signalEmitters[2].pointX == 0 && signalEmitters[2].pointY == -15.5 &&
                    std::abs(signalEmitters[2].pointZ - 122.2) < .001,
                "third red light mounted lower on upright");
        {
            collision::Mesh collider(traffic, 0);
            collision::Pose pose{{0, 0, 0}, 1, 0};
            require(collider.sweep(pose, {0, -50, 0}, {0, 50, 0}, 16, 56).hit,
                    "signal pole collision");
            require(!collider.sweep(pose, {50, -50, 0}, {50, 50, 0}, 16, 56).hit,
                    "walk beneath signal housing");
            require(collider.ray(pose, {136, -40, 179}, {136, 10, 179}).hit,
                    "signal housing blocks shots");
            require(collider.ray(pose, {256, -40, 179}, {256, 10, 179}).hit,
                    "far signal housing blocks shots");
            require(collider.ray(pose, {0, -40, 110}, {0, -5, 110}).hit,
                    "pole-mounted housing blocks shots");
            auto defs = doom_materials::parse(
                "DOOM_MATERIALS 1\nW TESTRED 64 64 EMISSIVE 1 0 0 2 420 BLINK 35 0\n");
            defs = doom_materials::parse(doom_materials::serialize(defs));
            require(defs[0].emission.blink && defs[0].emission.strobePeriod == 35,
                    "BLINK metadata roundtrip");
        }
        const auto *palm = registry.find(15001);
        require(palm != nullptr, "palm registered");
        const auto &tree = registry.load(15001);
        require(tree.frames == 1 && tree.surfaces.size() == 4, "palm trunk and three frond tiers");
        for (const auto &surface : tree.surfaces) {
            require(
                std::filesystem::exists(relativeAsset(palm->file.parent_path(), surface.shader)),
                "palm skin exists");
            require(!surface.fullbright(), "palm uses scene lighting");
            for (const auto &v : surface.frames[0])
                require(v.position.z >= 0 && v.position.z <= palm->height,
                        "palm fits declared height");
        }
        const auto *car = registry.find(15005);
        require(car != nullptr, "police car registered");
        const auto &sedan = registry.load(15005);
        require(sedan.frames == 1 && sedan.surfaces.size() >= 10,
                "police car has detailed static surfaces");
        unsigned attachments = 0;
        doom_materials::Emission red, blue;
        for (const auto &surface : sedan.surfaces) {
            for (const auto &v : surface.frames[0])
                require(v.position.z >= 0 && v.position.z <= car->height,
                        "car fits floor and clearance");
            const auto e = skinEmission(relativeAsset(car->file.parent_path(), surface.shader));
            if (!e.point)
                continue;
            ++attachments;
            require(surface.fullbright() && e.strobePeriod, "point source has animated lens");
            if (e.red > e.blue)
                red = e;
            else
                blue = e;
        }
        require(attachments == 6, "roof, headlight and brake point attachments");
        require(car->height == 56, "lower sedan clearance");
        size_t carTriangles = 0;
        for (const auto &surface : sedan.surfaces)
            carTriangles += surface.triangles.size();
        require(carTriangles > 10000, "car has curved high-detail geometry");
        {
            collision::Mesh carMesh(sedan, unsigned(car->first));
            collision::Pose pose{{0, 0, 0}, 1, 0};
            require(carMesh.sweep(pose, {0, 90, 0}, {0, 25, 0}, 16, 56).hit,
                    "car body blocks movement");
            require(carMesh.sweep(pose, {130, 0, 0}, {130, 0, 0}, 16, 56).hit,
                    "car nose collides beyond the old thing radius");
            require(!carMesh.sweep(pose, {155, 0, 0}, {155, 0, 0}, 16, 56).hit,
                    "clear space beyond car remains open");
            pose.angle = 3.14159265f / 2;
            require(carMesh.sweep(pose, {0, 130, 0}, {0, 130, 0}, 16, 56).hit,
                    "car collision follows yaw");
            const auto *fence = registry.find(15002);
            require(fence != nullptr, "fence registered");
            collision::Mesh panel(registry.load(15002), 0);
            pose.angle = 0;
            require(panel.sweep(pose, {55, -50, 0}, {55, 50, 0}, 16, 56).hit,
                    "zero-radius fence blocks its full width and swept movement");
            require(!panel.ray(pose, {0, -20, 65}, {0, 20, 65}).hit,
                    "shots pass through real fence gaps");
            require(panel.ray(pose, {8, -20, 65}, {8, 20, 65}).hit, "shots hit fence pickets");
            collision::Mesh streetlight(model, 0);
            require(!streetlight.sweep(pose, {45, -20, 0}, {45, 0, 0}, 16, 56).hit,
                    "walking beneath the streetlight arm stays clear");
            require(streetlight.sweep(pose, {45, 0, 180}, {45, 0, 180}, 16, 56).hit,
                    "raised actor meets overhanging geometry");
            collision::Mesh palmMesh(tree, 0);
            require(palmMesh.sweep(pose, {0, 0, 0}, {0, 0, 0}, 16, 56).hit, "palm trunk is solid");
            require(!palmMesh.sweep(pose, {100, 0, 0}, {100, 0, 0}, 16, 56).hit,
                    "palm crown does not block ground beneath it");
            auto support = carMesh.ray({{0, 0, 0}, 1, 0}, {70, 0, 60}, {70, 0, 0}, true);
            require(support.hit, "car hood supports a downward landing");
        }
        const auto headlights = skinEmission(car->file.parent_path() / "head-left.png");
        const auto brakes = skinEmission(car->file.parent_path() / "brake-left.png");
        require(headlights.point && brakes.point && headlights.strobePeriod == 20 &&
                    brakes.strobePeriod == 20,
                "headlights and brake lights have synchronized flash metadata");
        require(headlights.pointX > 110 && brakes.pointX < -110,
                "front/rear lamp attachments outside the body");
        for (unsigned seed = 0; seed < 32; ++seed)
            for (unsigned carIndex = 0; carIndex < 8; carIndex += 2) {
                const auto first = carFlashPhase(seed, carIndex),
                           second = carFlashPhase(seed, carIndex + 1);
                require(first + second == 10 && first != second,
                        "spawn pairs start with opposite colors");
            }
        require(doom_materials::emissionBrightness(red, 0) == 1 &&
                    doom_materials::emissionBrightness(blue, 0) == 0,
                "red phase");
        require(doom_materials::emissionBrightness(red, 10) == 0 &&
                    doom_materials::emissionBrightness(blue, 10) == 1,
                "blue phase");
        require(doom_materials::emissionBrightness(red, 3) == 0 &&
                    doom_materials::emissionBrightness(blue, 3) == 0,
                "dark gap");
        require(doom_materials::emissionBrightness(red, 4) == 1, "second red pulse");
        for (unsigned tick = 0; tick < 700; ++tick) {
            require(doom_materials::emissionBrightness(red, tick) +
                            doom_materials::emissionBrightness(blue, tick) <=
                        1,
                    "red and blue alternate");
            require(doom_materials::emissionBrightness(red, tick) ==
                        doom_materials::emissionBrightness(red, tick + 20),
                    "stable repeating flash timing");
        }
        const auto attachment = pointAttachment(red, 2, 3.14159265f / 2, {100, 200, 30});
        require(std::abs(attachment.x - 168) < .001 && std::abs(attachment.y - 200) < .001 &&
                    std::abs(attachment.z - 135) < .001,
                "point attachment follows yaw, scale and elevation");
        for (const auto *bad :
             {"EMISSIVE 1 0 0 2 340 STROBE 0 0", "EMISSIVE 1 0 0 2 340 STROBE 20 20",
              "EMISSIVE 1 0 0 2 340 POINT 0 0", "EMISSIVE 1 0 0 2 340 STROBE 20 0 FLICKER"}) {
            std::istringstream row(bad);
            std::string prefix;
            row >> prefix;
            bool failed = false;
            try {
                doom_materials::parseEmission(row);
            } catch (...) {
                failed = true;
            }
            require(failed, "invalid flash and attachment metadata rejected");
        }
        auto rejects = [&](std::vector<uint8_t> data) {
            bool rejected = false;
            try {
                decode(data);
            } catch (const std::exception &) {
                rejected = true;
            }
            require(rejected, "malformed MD3 accepted");
        };
        auto corrupt = bytes;
        corrupt.resize(80);
        rejects(corrupt);
        corrupt = bytes;
        corrupt[4] = 14;
        rejects(corrupt);
        corrupt = bytes;
        corrupt[100] = 0xff;
        rejects(corrupt);
        corrupt = bytes;
        corrupt[164 + 80] = 0;
        corrupt[164 + 81] = 0;
        rejects(corrupt);
        corrupt = bytes;
        for (int i = 0; i < 4; ++i)
            corrupt[164 + 108 + i] = 255;
        rejects(corrupt);
        corrupt = bytes;
        corrupt.pop_back();
        rejects(corrupt);
        bool rejected = false;
        try {
            relativeAsset(root, "../escape.png");
        } catch (...) {
            rejected = true;
        }
        require(rejected, "parent traversal rejected");
        auto animated = model.surfaces[0];
        animated.frames.push_back(animated.frames[0]);
        for (auto &v : animated.frames[1])
            v.position.z += 10;
        auto anim = *d;
        anim.first = 0;
        anim.last = 1;
        anim.fps = 1;
        auto middle = sample(animated, anim, 0, .5);
        require(std::abs(middle.position.z - animated.frames[0][0].position.z - 5) < .001,
                "frame interpolation");
        auto rotated = rotate({1, 0, 0}, 3.14159265f / 2);
        require(std::abs(rotated.x) < .001 && std::abs(rotated.y - 1) < .001,
                "Doom angle rotation");

        // Round-trip a model thing through the editor's real document format.
        de::EditWorld world;
        world.load(root / "tools/DoomEd/tests/fixtures/AttachedInner.dwd");
        de::Thing t;
        t.type = 15005;
        t.origin = {64, 64};
        t.angle = 37;
        t.options = 7;
        world.things.push_back(t);
        world.save(out / "model-roundtrip.dwd");
        de::EditWorld roundtrip;
        roundtrip.load(out / "model-roundtrip.dwd");
        require(roundtrip.things.back().type == 15005 && roundtrip.things.back().angle == 37,
                "police car save roundtrip");
        std::cout << "MD3 regression passed: decoding, bounds, invalid input, skins, animation, "
                     "rotation, map persistence\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
