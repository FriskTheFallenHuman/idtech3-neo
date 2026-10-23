#pragma once
#include "Wadfile.h"
namespace de {
struct DoomProject {
    std::filesystem::path pathname, wadpath, mapwads, bspProgram;
    std::string bspHost = "localhost";
    std::vector<std::string> maps;
    std::vector<Texture> textures;
    Wadfile wad;
    std::vector<std::filesystem::path> materialPacks;
    void load(const std::filesystem::path &path);
    void save(const std::filesystem::path &path);
    void loadWad(const std::filesystem::path &path);
    void loadMapTextures(const std::filesystem::path &directory);
    void loadMaterialPack(const std::filesystem::path &path);
    void loadTextures(const std::filesystem::path &path, int set);
    void saveTextures(const std::filesystem::path &path, int set) const;
    void exportTextures(const std::filesystem::path &path) const;
};
} // namespace de
