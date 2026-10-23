#include "DoomProject.h"
#include "idfunctions.h"
#include <algorithm>

namespace de {
std::vector<Texture> Wadfile::surfaceTextures() const {
    std::map<std::string, Texture> catalog;
    for (auto &texture : textures(false)) {
        catalog[texture.name] = texture;
    }
    // Classic storage kind is an image decoder detail, never a surface restriction.
    bool inFlats = false;
    for (const auto &lump : lumps) {
        if (lump.name == "F_START" || lump.name == "FF_START") {
            inFlats = true;
        } else if (lump.name == "F_END" || lump.name == "FF_END") {
            inFlats = false;
        } else if (inFlats && lump.data.size() == 4096 && !catalog.count(lump.name)) {
            Texture texture;
            texture.name = lump.name;
            texture.width = texture.height = 64;
            texture.wadIndex = 3;
            catalog[texture.name] = texture;
        }
    }
    for (const auto &definition : materials()) {
        Texture texture;
        texture.name = definition.name;
        texture.width = definition.width;
        texture.height = definition.height;
        texture.wadIndex = 2;
        catalog[texture.name] = texture;
    }
    if (!mapImages.empty()) {
        for (auto &texture : textures()) {
            catalog[texture.name] = texture;
        }
    }
    std::vector<Texture> result;
    for (const auto &[name, texture] : catalog) {
        result.push_back(texture);
    }
    return result;
}

Image Wadfile::surfaceImage(const std::string &name) const {
    const auto key = upper(name);
    if (mapImages.count(key)) {
        return materialImage(key);
    }
    for (const auto &definition : materials()) {
        if (definition.name == key) {
            return materialImage(key);
        }
    }
    for (const auto &texture : textures(false)) {
        if (texture.name == key) {
            return this->texture(texture);
        }
    }
    return flat(key);
}
void Wadfile::loadMapTextures(const std::filesystem::path &directory) {
    std::map<std::string, std::filesystem::path> next;
    if (std::filesystem::is_directory(directory)) {
        for (const auto &entry : std::filesystem::directory_iterator(directory)) {
            if (!entry.is_regular_file() || upper(entry.path().extension().u8string()) != ".PNG") {
                continue;
            }
            const auto name = upper(entry.path().stem().u8string());
            // Cube faces are generated sky assets, not assignable surface materials.
            if (name.rfind("_SKY_", 0) == 0) {
                continue;
            }
            require(!name.empty() && name.size() <= 11 &&
                        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
                            std::string::npos,
                    "PNG texture names must use 1..11 letters, digits, underscores or hyphens: " +
                        entry.path().u8string());
            require(next.emplace(name, entry.path()).second,
                    "Duplicate PNG texture name: " + entry.path().u8string());
        }
    }
    mapImages = std::move(next);
    textureDirectory = directory;
}

void DoomProject::loadMapTextures(const std::filesystem::path &directory) {
    auto next = wad;
    next.loadMapTextures(directory);
    auto catalog = next.surfaceTextures();
    wad = std::move(next);
    textures = std::move(catalog);
}
} // namespace de
