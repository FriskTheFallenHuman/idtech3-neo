// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdio>
#include <filesystem>

const std::filesystem::path &compilerProjectRoot();
const std::filesystem::path &compilerLogPath();
void compilerCheckCancelled();
std::FILE *compilerOpenFile(const char *path, const char *mode);
std::FILE *compilerOpenFile(const wchar_t *path, const wchar_t *mode);
int compilerCloseFile(std::FILE *file);
void aasMemoryBegin();
void aasMemoryEnd();
