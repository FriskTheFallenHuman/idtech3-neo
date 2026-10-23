#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace doom_sprites {
struct Frame {
    int state = 0, rotation = 1, originalFrame = 0;
    std::string sprite, file;
    float width = 0, height = 0, left = 0, top = 0;
    std::filesystem::path directory;
};
inline bool identifier(const std::string &s) {
    return !s.empty() && s.size() <= 64 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}
inline int key(int state, int rotation) {
    return state * 8 + rotation - 1;
}
inline std::vector<Frame> parse(std::istream &input) {
    std::string magic;
    int version = 0;
    if (!(input >> magic >> version) || magic != "DOOMSPRITES" || version != 1)
        throw std::runtime_error("Unsupported sprite manifest");
    std::vector<Frame> frames;
    std::map<int, bool> used;
    while (input >> std::ws && input.peek() != EOF) {
        Frame f;
        if (!(input >> f.state >> f.sprite >> f.originalFrame >> f.rotation >>
              std::quoted(f.file) >> f.width >> f.height >> f.left >> f.top))
            throw std::runtime_error("Malformed sprite frame");
        if (f.state < 1 || f.state > 100000 || f.sprite.size() != 4 || !identifier(f.sprite) ||
            f.originalFrame < 0 || f.originalFrame > 31 || f.rotation < 1 || f.rotation > 8 ||
            f.file.size() < 5 || f.file.substr(f.file.size() - 4) != ".png" ||
            !identifier(f.file.substr(0, f.file.size() - 4)) || !std::isfinite(f.width) ||
            !std::isfinite(f.height) || !std::isfinite(f.left) || !std::isfinite(f.top) ||
            f.width <= 0 || f.height <= 0 || f.width > 4096 || f.height > 4096 ||
            std::abs(f.left) > 4096 || std::abs(f.top) > 4096 ||
            !used.emplace(key(f.state, f.rotation), true).second || frames.size() >= 8192)
            throw std::runtime_error("Invalid or duplicate sprite frame");
        frames.push_back(f);
    }
    if (frames.empty())
        throw std::runtime_error("Empty sprite manifest");
    for (const auto &f : frames)
        for (int r = 1; r <= 8; ++r)
            if (!used.count(key(f.state, r)))
                throw std::runtime_error("Sprite state needs all eight angles");
    std::map<std::string, Frame> images;
    std::map<int, Frame> states;
    for (const auto &f : frames) {
        const auto [image, fresh] = images.emplace(f.file, f);
        const auto &old = image->second;
        if (!fresh && (old.width != f.width || old.height != f.height || old.left != f.left ||
                       old.top != f.top))
            throw std::runtime_error("Shared sprite image has inconsistent scale or origin");
        const auto [state, newState] = states.emplace(f.state, f);
        if (!newState &&
            (state->second.sprite != f.sprite || state->second.originalFrame != f.originalFrame))
            throw std::runtime_error("Sprite state has inconsistent original frame");
    }
    return frames;
}
inline void write(std::ostream &out, const std::vector<Frame> &frames) {
    out << "DOOMSPRITES 1\n" << std::setprecision(9);
    for (const auto &f : frames)
        out << f.state << ' ' << f.sprite << ' ' << f.originalFrame << ' ' << f.rotation << ' '
            << std::quoted(f.file) << ' ' << f.width << ' ' << f.height << ' ' << f.left << ' '
            << f.top << '\n';
    if (!out)
        throw std::runtime_error("Cannot write sprite manifest");
}
inline std::map<int, Frame> discover(const std::filesystem::path &root) {
    std::map<int, Frame> result;
    if (!std::filesystem::exists(root))
        return result;
    std::vector<std::filesystem::path> packs;
    for (const auto &entry : std::filesystem::directory_iterator(root))
        if (entry.is_directory() && entry.path().filename().u8string().front() != '.' &&
            std::filesystem::exists(entry.path() / "sprites.dspr") &&
            !std::filesystem::exists(entry.path() / "disabled"))
            packs.push_back(entry.path());
    std::sort(packs.begin(), packs.end());
    for (const auto &pack : packs) {
        std::ifstream input(pack / "sprites.dspr");
        for (auto f : parse(input)) {
            f.directory = pack;
            if (!std::filesystem::is_regular_file(pack / f.file))
                throw std::runtime_error("Missing sprite PNG: " + (pack / f.file).u8string());
            if (!result.emplace(key(f.state, f.rotation), f).second)
                throw std::runtime_error("Conflicting active sprite packs: " + pack.u8string());
        }
    }
    return result;
}
} // namespace doom_sprites
