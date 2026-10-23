#pragma once
#include "R_mapdef.h"
#include <filesystem>
#include <map>
#include "formats/material_manifest.h"
namespace de {
struct Lump {
    std::string name;
    Bytes data;
};
class Wadfile {
  public:
    std::vector<Lump> lumps;
    std::filesystem::path textureDirectory;
    std::map<std::string, std::filesystem::path> mapImages;
    void loadMapTextures(const std::filesystem::path &directory);
    void load(const std::filesystem::path &p);
    void save(const std::filesystem::path &p) const;
    void saveWithMaterials(const std::filesystem::path &p, const Wadfile &resources) const;
    int find(const std::string &name) const;
    const Bytes &get(const std::string &name) const;
    void add(const std::string &name, Bytes data);
    std::vector<std::string> mapNames() const;
    std::vector<std::string> flatNames(bool hdOnly = false) const;
    std::vector<std::string> patchNames() const;
    std::vector<Texture> textures(bool mapCatalog = true) const;
    std::vector<Texture> surfaceTextures() const;
    Image surfaceImage(const std::string &name) const;
    std::array<uint32_t, 256> palette() const;
    Image flat(const std::string &name) const;
    Image patch(const std::string &name) const;
    Image texture(const Texture &t) const;
    std::vector<doom_materials::Definition> materials() const;
    Image materialImage(const std::string &name) const;
    void mergeMaterials(const Wadfile &pack);
    void copyMaterialsTo(Wadfile &target) const;
};
} // namespace de
