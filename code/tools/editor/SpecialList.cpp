#include "SpecialList.h"
#include "idfunctions.h"
#include <sstream>
namespace de {
void SpecialList::load(const std::filesystem::path &p) {
    auto b = readFile(p);
    std::istringstream in(std::string(b.begin(), b.end()));
    std::string row;
    std::getline(in, row);
    int count;
    require(sscanf(row.c_str(), "numspecials: %d", &count) == 1 && count >= 0 && count <= 65536,
            "Invalid specials count");
    std::vector<Special> list;
    for (int i = 0; i < count; ++i) {
        require(bool(std::getline(in, row)), "Truncated specials file");
        auto colon = row.find(':');
        require(colon != std::string::npos, "Invalid special");
        list.push_back({std::stoi(row.substr(0, colon)), row.substr(colon + 1)});
    }
    entries = std::move(list);
}
void SpecialList::save(const std::filesystem::path &p) const {
    std::ostringstream out;
    out << "numspecials: " << entries.size() << '\n';
    for (auto &s : entries)
        out << s.value << ':' << s.description << '\n';
    writeTextAtomic(p, out.str());
}
} // namespace de
