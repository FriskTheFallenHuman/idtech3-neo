// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompilerAPI.h"
#include "CompilerJob.h"
#include "CompilerRuntime.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int DmapMain(int argc, char **argv);
int AasBuildMain(int argc, char **argv);
void Log_Shutdown();
void aasThreadsEnd();
namespace {
std::filesystem::path projectRoot, logPath;
const DoomCompilerRequest *activeRequest = nullptr;
std::atomic_flag used = ATOMIC_FLAG_INIT;
std::vector<std::FILE *> compilerFiles;
std::FILE *trackCompilerFile(std::FILE *file) {
    if (file) {
        try {
            compilerFiles.push_back(file);
        } catch (...) {
            std::fclose(file);
            throw;
        }
    }
    return file;
}
} // namespace
std::FILE *compilerOpenFile(const char *path, const char *mode) {
    return trackCompilerFile(std::fopen(path, mode));
}
std::FILE *compilerOpenFile(const wchar_t *path, const wchar_t *mode) {
    return trackCompilerFile(_wfopen(path, mode));
}
int compilerCloseFile(std::FILE *file) {
    const auto found = std::find(compilerFiles.begin(), compilerFiles.end(), file);
    if (found != compilerFiles.end()) {
        compilerFiles.erase(found);
    }
    return std::fclose(file);
}
const std::filesystem::path &compilerProjectRoot() {
    return projectRoot;
}
const std::filesystem::path &compilerLogPath() {
    return logPath;
}
void compilerCheckCancelled() {
    if (activeRequest && activeRequest->cancelled &&
        activeRequest->cancelled(activeRequest->context)) {
        throw std::runtime_error("Compilation cancelled");
    }
}
extern "C" __declspec(dllexport) int DoomTools_Compile(const DoomCompilerRequest *request) {
    if (!request || request->version != DoomCompilerAPIVersion ||
        request->size != sizeof(DoomCompilerRequest) || !request->projectRoot ||
        !request->logPath || request->argumentCount < 1 || request->argumentCount > 256 ||
        !request->arguments || used.test_and_set()) {
        return 1;
    }
    int result = 1;
    std::ofstream log;
    std::streambuf *previousOut = nullptr, *previousError = nullptr;
    bool arena = false;
    try {
        projectRoot = request->projectRoot;
        logPath = request->logPath;
        log.open(logPath, std::ios::app);
        if (!log) {
            throw std::runtime_error("Cannot open compiler log");
        }
        previousOut = std::cout.rdbuf(log.rdbuf());
        previousError = std::cerr.rdbuf(log.rdbuf());
        activeRequest = request;
        std::vector<std::string> arguments;
        std::vector<char *> pointers;
        for (int i = 0; i < request->argumentCount; ++i) {
            if (!request->arguments[i]) {
                throw std::runtime_error("Invalid compiler argument");
            }
            arguments.emplace_back(request->arguments[i]);
        }
        for (auto &argument : arguments) {
            pointers.push_back(argument.data());
        }
        log << "\n> " << arguments.front() << '\n' << std::flush;
        compilerCheckCancelled();
        if (arguments.front() == "dmap") {
            result = DmapMain(static_cast<int>(pointers.size()), pointers.data());
        } else if (arguments.front() == "aasbuild") {
            aasMemoryBegin();
            arena = true;
            result = AasBuildMain(static_cast<int>(pointers.size()), pointers.data());
        } else {
            throw std::runtime_error("Unknown compiler: " + arguments.front());
        }
    } catch (const std::exception &error) {
        if (log) {
            log << "Compiler error: " << error.what() << '\n';
        }
    } catch (...) {
        if (log) {
            log << "Unknown compiler failure\n";
        }
    }
    Log_Shutdown();
    if (arena) {
        aasThreadsEnd();
        aasMemoryEnd();
    }
    activeRequest = nullptr;
    if (previousOut) {
        std::cout.rdbuf(previousOut);
    }
    if (previousError) {
        std::cerr.rdbuf(previousError);
    }
    // A fatal legacy parse can leave files open. Close only this invocation's
    // files: MinGW shares UCRT's FILE table with the engine and other DLLs.
    for (auto *file : compilerFiles) {
        std::fclose(file);
    }
    compilerFiles.clear();
    log.close();
    return result;
}

// Windows' existing 32-bit DLL host is useful for offline scripts on machines
// with 64-bit Python. The game/editor call the compiler directly on a worker.
// A UTF-8 response file contains project root, log path, then one argument/line.
extern "C" __declspec(dllexport) void CALLBACK DoomTools_RunW(HWND, HINSTANCE, LPWSTR command,
                                                              int) {
    std::filesystem::path response(command);
    if (response.native().size() >= 2 && response.native().front() == L'"' &&
        response.native().back() == L'"') {
        response = response.native().substr(1, response.native().size() - 2);
    }
    int result = 1;
    try {
        std::ifstream input(response, std::ios::binary);
        std::vector<std::string> lines;
        for (std::string line; std::getline(input, line);) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
        if (!input.eof() || lines.size() < 3) {
            throw std::runtime_error("Invalid compiler response file");
        }
        auto *job =
            startCompilerJob(std::filesystem::u8path(lines[0]), response.parent_path() / "context",
                             std::filesystem::u8path(lines[1]), {lines.begin() + 2, lines.end()});
        result = finishCompilerJob(job);
    } catch (const std::exception &error) {
        auto errorPath = response;
        errorPath += ".error";
        std::ofstream(errorPath) << error.what();
    }
    auto resultPath = response;
    resultPath += ".exit";
    std::ofstream(resultPath) << result;
}
