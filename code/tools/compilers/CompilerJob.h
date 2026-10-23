// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct CompilerJob;
CompilerJob *startCompilerJob(const std::filesystem::path &root,
                              const std::filesystem::path &scratch,
                              const std::filesystem::path &log, std::vector<std::string> arguments);
bool compilerJobFinished(const CompilerJob *job);
int finishCompilerJob(CompilerJob *job);
void cancelCompilerJob(CompilerJob *job);
