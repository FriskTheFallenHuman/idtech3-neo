// SPDX-License-Identifier: GPL-3.0-or-later
#include "qcommon/doom_bsp.h"
#include <cstring>
#include <limits>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Pass a valid doommap BSP fixture");
        std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Cannot open BSP fixture");
        auto size = file.tellg();
        std::vector<uint8_t> original(static_cast<size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char *>(original.data()), size);
        if (const char *error = dbsp::validate(original.data(), int(original.size())))
            throw std::runtime_error(error);
        int checks = 0;
        auto reject = [&](auto mutate) {
            auto data = original;
            auto &h = *reinterpret_cast<dbsp::Header *>(data.data());
            mutate(h, data);
            if (!dbsp::validate(data.data(), int(data.size())))
                throw std::runtime_error("Malformed BSP was accepted");
            ++checks;
        };
        reject([](auto &h, auto &) { h.ident = 0; });
        reject([](auto &h, auto &) { h.version = 47; });
        reject([](auto &h, auto &) { h.lumps[dbsp::Sectors].offset = -1; });
        reject([](auto &h, auto &) { h.lumps[dbsp::Sectors].length = INT32_MAX; });
        reject([](auto &h, auto &) { h.lumps[dbsp::Sides].length -= 1; });
        reject(
            [](auto &h, auto &) { h.lumps[dbsp::Shaders].offset = h.lumps[dbsp::Planes].offset; });
        reject([](auto &h, auto &data) {
            auto *sector =
                reinterpret_cast<dbsp::Sector *>(data.data() + h.lumps[dbsp::Sectors].offset);
            sector->light = 256;
        });
        reject([](auto &h, auto &data) {
            auto *side = reinterpret_cast<dbsp::Side *>(data.data() + h.lumps[dbsp::Sides].offset);
            side->sector = INT32_MAX;
        });
        reject([](auto &h, auto &data) {
            auto *node = reinterpret_cast<dbsp::Node *>(data.data() + h.lumps[dbsp::Nodes].offset);
            node->children[0] = 0;
        });
        reject([](auto &h, auto &data) {
            auto *index = reinterpret_cast<int32_t *>(data.data() + h.lumps[dbsp::Indices].offset);
            *index = INT32_MAX;
        });
        reject([](auto &h, auto &data) {
            auto *surface =
                reinterpret_cast<dbsp::Surface *>(data.data() + h.lumps[dbsp::Surfaces].offset);
            surface->lightmap = 0;
        });
        reject([](auto &h, auto &data) {
            auto *metadata = reinterpret_cast<dbsp::SurfaceSector *>(
                data.data() + h.lumps[dbsp::SurfaceSectors].offset);
            metadata->line = INT32_MAX;
        });
        reject([](auto &h, auto &data) {
            auto *line = reinterpret_cast<dbsp::Line *>(data.data() + h.lumps[dbsp::Lines].offset);
            line->a[0] = std::numeric_limits<float>::quiet_NaN();
        });
        if (!dbsp::validate(original.data(), 100))
            throw std::runtime_error("Truncated header was accepted");
        std::cout << "PASS: valid BSP plus " << checks + 1 << " malformed-format cases\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
