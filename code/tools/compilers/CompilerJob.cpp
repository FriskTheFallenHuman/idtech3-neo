// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompilerJob.h"
#include "CompilerAPI.h"
#include <windows.h>
#include <process.h>
#include <atomic>
#include <fstream>
#include <memory>
#include <stdexcept>

struct CompilerJob {
    std::filesystem::path root, modulePath, log;
    std::vector<std::string> arguments;
    std::atomic<bool> cancelled{false}, finished{false};
    HANDLE thread = nullptr;
    int result = 1;
};
namespace {
bool cancelled(void *context) {
    return static_cast<CompilerJob *>(context)->cancelled.load();
}
unsigned __stdcall compileThread(void *context) {
    auto &job = *static_cast<CompilerJob *>(context);
    HMODULE module = nullptr;
    try {
        module = LoadLibraryExW(job.modulePath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) {
            throw std::runtime_error("Cannot load compiler module (Windows error " +
                                     std::to_string(GetLastError()) + ")");
        }
        const auto run =
            reinterpret_cast<DoomCompilerRun>(GetProcAddress(module, "DoomTools_Compile"));
        if (!run) {
            throw std::runtime_error("Compiler module is missing DoomTools_Compile");
        }
        std::vector<const char *> arguments;
        for (const auto &argument : job.arguments) {
            arguments.push_back(argument.c_str());
        }
        const DoomCompilerRequest request{DoomCompilerAPIVersion,
                                          sizeof(DoomCompilerRequest),
                                          job.root.c_str(),
                                          job.log.c_str(),
                                          static_cast<int>(arguments.size()),
                                          arguments.data(),
                                          cancelled,
                                          &job};
        job.result = run(&request);
    } catch (const std::exception &error) {
        std::ofstream(job.log, std::ios::app) << "Compiler job: " << error.what() << '\n';
    }
    if (module) {
        FreeLibrary(module);
    }
    job.finished.store(true);
    return 0;
}
} // namespace

CompilerJob *startCompilerJob(const std::filesystem::path &root,
                              const std::filesystem::path &scratch,
                              const std::filesystem::path &log,
                              std::vector<std::string> arguments) {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&startCompilerJob), &self)) {
        throw std::runtime_error("Cannot locate tools module");
    }
    wchar_t filename[32768]{};
    const auto length = GetModuleFileNameW(self, filename, 32768);
    if (!length || length >= 32768 || arguments.empty()) {
        throw std::runtime_error("Invalid compiler module path or arguments");
    }
    auto job = std::make_unique<CompilerJob>();
    job->root = root;
    job->log = log;
    job->arguments = std::move(arguments);
    std::filesystem::create_directories(scratch);
    job->modulePath = scratch / "compiler-context.dll";
    // Legacy AAS globals are confined to this fresh DLL image. No compiler
    // process, shared working directory, renderer calls or engine allocators.
    std::filesystem::copy_file(filename, job->modulePath,
                               std::filesystem::copy_options::overwrite_existing);
    job->thread =
        reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 32 * 1024 * 1024, compileThread, job.get(),
                                                STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr));
    if (!job->thread) {
        throw std::runtime_error("Cannot start compiler worker thread");
    }
    return job.release();
}
bool compilerJobFinished(const CompilerJob *job) {
    return job->finished.load();
}
int finishCompilerJob(CompilerJob *job) {
    WaitForSingleObject(job->thread, INFINITE);
    CloseHandle(job->thread);
    const int result = job->result;
    std::error_code ignored;
    std::filesystem::remove(job->modulePath, ignored);
    delete job;
    return result;
}
void cancelCompilerJob(CompilerJob *job) {
    job->cancelled.store(true);
    finishCompilerJob(job);
}
