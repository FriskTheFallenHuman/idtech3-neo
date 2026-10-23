#include "idfunctions.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace de {
void require(bool c, const std::string &m) {
    if (!c)
        throw std::runtime_error(m);
}
Bytes readFile(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    require(bool(f), "Cannot open " + p.u8string());
    auto n = f.tellg();
    require(n >= 0 && n <= 512ll * 1024 * 1024, "File exceeds 512 MiB limit");
    Bytes b(static_cast<size_t>(n));
    f.seekg(0);
    if (!b.empty())
        f.read(reinterpret_cast<char *>(b.data()), b.size());
    require(bool(f), "Cannot read " + p.u8string());
    return b;
}
void writeFileAtomic(const std::filesystem::path &p,
                     const std::function<void(std::ostream &)> &write) {
    auto tmp = p;
    tmp += L".doomed-tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        require(bool(f), "Cannot write " + tmp.u8string());
        write(f);
        f.flush();
        require(bool(f), "Write failed: " + tmp.u8string());
    }
#ifdef _WIN32
    require(MoveFileExW(tmp.c_str(), p.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
            "Cannot replace " + p.u8string());
#else
    std::filesystem::rename(tmp, p);
#endif
}
void writeFileAtomic(const std::filesystem::path &p, const Bytes &b) {
    writeFileAtomic(p, [&](std::ostream &f) {
        if (!b.empty())
            f.write(reinterpret_cast<const char *>(b.data()), b.size());
    });
}
void writeTextAtomic(const std::filesystem::path &p, const std::string &s) {
    writeFileAtomic(p, Bytes(s.begin(), s.end()));
}
std::string upper(std::string s) {
    for (auto &c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string name8(const uint8_t *p) {
    size_t n = 0;
    while (n < 8 && p[n])
        ++n;
    return upper(std::string(reinterpret_cast<const char *>(p), n));
}
void putName(Bytes &b, const std::string &s) {
    require(s.size() <= 8, "Name longer than 8 characters: " + s);
    for (size_t i = 0; i < 8; ++i)
        b.push_back(i < s.size()
                        ? static_cast<uint8_t>(std::toupper(static_cast<unsigned char>(s[i])))
                        : 0);
}
uint16_t u16(const Bytes &b, size_t p) {
    require(p <= b.size() && b.size() - p >= 2, "Truncated 16-bit field");
    return b[p] | (uint16_t(b[p + 1]) << 8);
}
int16_t i16(const Bytes &b, size_t p) {
    return static_cast<int16_t>(u16(b, p));
}
uint32_t u32(const Bytes &b, size_t p) {
    require(p <= b.size() && b.size() - p >= 4, "Truncated 32-bit field");
    return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) |
           (uint32_t(b[p + 3]) << 24);
}
void put16(Bytes &b, int v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(unsigned(v) >> 8));
}
void put32(Bytes &b, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        b.push_back(uint8_t(v >> (i * 8)));
}
} // namespace de
