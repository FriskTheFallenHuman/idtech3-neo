#pragma once
#include "R_mapdef.h"
#include <filesystem>
#include <stdexcept>
#include <functional>
#include <iosfwd>
namespace de {
Bytes readFile(const std::filesystem::path &path);
void writeFileAtomic(const std::filesystem::path &path, const Bytes &data);
void writeFileAtomic(const std::filesystem::path &path,
                     const std::function<void(std::ostream &)> &write);
void writeTextAtomic(const std::filesystem::path &path, const std::string &text);
std::string upper(std::string s);
std::string name8(const uint8_t *p);
void putName(Bytes &data, const std::string &name);
uint16_t u16(const Bytes &b, size_t p);
int16_t i16(const Bytes &b, size_t p);
uint32_t u32(const Bytes &b, size_t p);
void put16(Bytes &b, int v);
void put32(Bytes &b, uint32_t v);
void require(bool condition, const std::string &message);
} // namespace de
