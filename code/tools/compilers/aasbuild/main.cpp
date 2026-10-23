// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../../qcommon/doom_bsp.h"
#include "../CompilerRuntime.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int AasBuildLegacyMain(int argc, char **argv);
namespace {
namespace fs = std::filesystem;
void require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
void validateBsp(const fs::path &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(bool(input), "Cannot open BSP: " + path.string());
    const auto size = input.tellg();
    require(size > 0 && size <= 512ll * 1024 * 1024, "BSP size must be 1..512 MiB");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char *>(bytes.data()), size);
    require(bool(input), "Cannot read BSP: " + path.string());
    const auto *error = dbsp::validate(bytes.data(), static_cast<int>(bytes.size()));
    require(!error, std::string("Invalid Doom BSP: ") + (error ? error : ""));
}
} // namespace

int AasBuildMain(int argc, char **argv) {
    try {
        const std::array<std::string, 4> profiles = {"small", "medium", "cyber", "spider"};
        std::string profile = "small";
        fs::path source, output;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help" || option == "-h") {
                std::cout << "aasbuild [--profile small|medium|cyber|spider] "
                             "[--output <directory>] <map.bsp>\n"
                             "Builds one Doom navigation world (_b0.._b3). "
                             "Run all four profiles for a playable map.\n";
                return 0;
            }
            if (option == "--profile" || option == "--output") {
                require(i + 1 < argc, "Missing value for " + option);
                if (option == "--profile") {
                    profile = argv[++i];
                } else {
                    output = fs::absolute(argv[++i]);
                }
            } else {
                require(!option.empty() && option.front() != '-', "Unknown option: " + option);
                require(source.empty(), "Specify exactly one BSP; wildcards are not supported");
                source = fs::absolute(option);
            }
        }
        require(!source.empty(), "Specify a Doom BSP; use --help for usage");
        const auto found = std::find(profiles.begin(), profiles.end(), profile);
        require(found != profiles.end(), "Unknown navigation profile: " + profile);
        const auto name = source.stem().string();
        require(!name.empty() && name.size() <= 32 &&
                    name.find_first_not_of(
                        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") ==
                        std::string::npos,
                "Map names must contain 1..32 letters, digits or underscores");
        auto extension = source.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        require(extension == ".bsp", "aasbuild accepts compiled Doom .bsp files");
        if (output.empty()) {
            output = source.parent_path();
        }
        const auto config = compilerProjectRoot() / "base/aas" / ("doom_" + profile + ".cfg");
        require(fs::is_regular_file(config), "Missing profile: " + config.string());
        // Legacy algorithms retain MAX_PATH buffers and an ANSI file interface.
        // Refuse unrepresentable/oversized input before entering that code.
        for (const auto &path : {source, output, config}) {
            const auto narrow = path.string();
            require(narrow.size() < 200 && narrow.find_first_of("*?\r\n") == std::string::npos &&
                        fs::path(narrow) == path,
                    "Compiler paths must round-trip through ANSI and be shorter than 200 bytes");
        }
        validateBsp(source);
        fs::create_directories(output);
        std::vector<std::string> arguments = {"aasbuild",
                                              "-cfg",
                                              config.string(),
                                              "-ext",
                                              "_b" + std::to_string(found - profiles.begin()),
                                              "-output",
                                              output.string(),
                                              "-optimize",
                                              "-threads",
                                              "1",
                                              "-forcesidesvisible",
                                              "-bsp2aas",
                                              source.string()};
        std::vector<char *> pointers;
        for (auto &argument : arguments) {
            pointers.push_back(argument.data());
        }
        // Winding allocation is not thread safe. Parallelism belongs between
        // independent module contexts, never inside one navigation world.
        return AasBuildLegacyMain(static_cast<int>(pointers.size()), pointers.data());
    } catch (const std::exception &error) {
        std::cerr << "aasbuild: " << error.what() << '\n';
        return 1;
    }
}
