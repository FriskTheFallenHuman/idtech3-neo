#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace de {
struct Special {
    int value = 0;
    std::string description;
};
struct SpecialList {
    std::vector<Special> entries;
    void load(const std::filesystem::path &p);
    void save(const std::filesystem::path &p) const;
};
} // namespace de
