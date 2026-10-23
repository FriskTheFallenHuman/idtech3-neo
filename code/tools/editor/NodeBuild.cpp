#include <windows.h>
#include <wincrypt.h>
#include "NodeBuild.h"
#include "../compilers/CompilerJob.h"
#include "NextUI.h"
#include "idfunctions.h"
#include "../../qcommon/doom_bsp.h"
#include <fstream>
#include <iterator>
#include <optional>

namespace de {
namespace {
std::filesystem::path configuredRoot;
std::wstring quote(const std::wstring &value) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (const auto c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        result.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        result += c;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}
std::string lowerName(std::string name) {
    for (auto &c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return name;
}
HANDLE startProcess(const std::filesystem::path &program,
                    const std::vector<std::wstring> &arguments,
                    const std::filesystem::path &directory, const std::filesystem::path &log) {
    require(std::filesystem::is_regular_file(program), "Cannot find tool: " + program.u8string());
    std::wstring command = quote(program.wstring());
    for (const auto &argument : arguments) {
        command += L" " + quote(argument);
    }
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE output = CreateFileW(log.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                               OPEN_EXISTING, 0, nullptr);
    if (output == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (output != INVALID_HANDLE_VALUE) {
            CloseHandle(output);
        }
        if (input != INVALID_HANDLE_VALUE) {
            CloseHandle(input);
        }
        throw std::runtime_error("Cannot open process input/log: " + log.u8string() +
                                 " (Windows error " + std::to_string(error) + ")");
    }
    const auto heading = "\r\n> " + program.filename().u8string() + "\r\n";
    DWORD written = 0;
    const bool logged =
        WriteFile(output, heading.data(), static_cast<DWORD>(heading.size()), &written, nullptr) &&
        written == heading.size();
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = startup.hStdError = output;
    startup.hStdInput = input;
    PROCESS_INFORMATION process{};
    const BOOL started =
        logged && CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, TRUE,
                                 CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process);
    const auto error = GetLastError();
    CloseHandle(output);
    CloseHandle(input);
    require(started != 0, "Cannot start " + program.u8string() + " (Windows error " +
                              std::to_string(error) + ")");
    CloseHandle(process.hThread);
    return process.hProcess;
}
uint32_t bspChecksum(const Bytes &bytes) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    require(CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) !=
                0,
            "Cannot initialize BSP checksum provider");
    struct Scope {
        HCRYPTPROV provider;
        HCRYPTHASH &hash;
        ~Scope() {
            if (hash) {
                CryptDestroyHash(hash);
            }
            CryptReleaseContext(provider, 0);
        }
    } scope{provider, hash};
    require(CryptCreateHash(provider, CALG_MD4, 0, 0, &hash) &&
                CryptHashData(hash, bytes.data(), static_cast<DWORD>(bytes.size()), 0),
            "Cannot hash compiled BSP");
    std::array<uint32_t, 4> digest{};
    DWORD size = sizeof(digest);
    require(
        CryptGetHashParam(hash, HP_HASHVAL, reinterpret_cast<BYTE *>(digest.data()), &size, 0) &&
            size == sizeof(digest),
        "Cannot finish BSP checksum");
    return digest[0] ^ digest[1] ^ digest[2] ^ digest[3];
}
void validateAas(const std::filesystem::path &path, uint32_t checksum) {
    auto bytes = readFile(path);
    require(bytes.size() >= 124 && memcmp(bytes.data(), "EAAS", 4) == 0 && u32(bytes, 4) == 8,
            "Invalid AAS header: " + path.u8string());
    for (size_t i = 8; i < 124; ++i) {
        bytes[i] ^= static_cast<uint8_t>((i - 8) * 119);
    }
    require(u32(bytes, 8) == checksum, "AAS checksum does not match BSP: " + path.u8string());
    for (size_t i = 0; i < 14; ++i) {
        const size_t offset = u32(bytes, 12 + i * 8), length = u32(bytes, 16 + i * 8);
        require(offset <= bytes.size() && length <= bytes.size() - offset,
                "Invalid AAS lump bounds: " + path.u8string());
    }
    // Large monster profiles can legitimately have no navigable areas in a small room.
}
} // namespace

std::filesystem::path projectDirectory() {
    if (!configuredRoot.empty()) {
        return configuredRoot;
    }
    wchar_t executable[32768]{};
    const auto length =
        GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    require(length && length < std::size(executable), "Cannot locate engine executable");
    auto directory = std::filesystem::path(executable).parent_path();
    return upper(directory.filename().u8string()) == "TOOLS" ? directory.parent_path() : directory;
}
void setProjectDirectory(const std::filesystem::path &path) {
    configuredRoot = std::filesystem::absolute(path).lexically_normal();
}
void App::refreshMapTextures() {
    const auto directory = projectDirectory() / "base/textures" / lowerName(mapName);
    if (project.wad.textureDirectory == directory) {
        return;
    }
    // Record failed attempts too, so a bad PNG produces one actionable error, not a dialog per frame.
    project.wad.textureDirectory = directory;
    action([&] {
        project.loadMapTextures(directory);
        selectedTexture = project.textures.empty() ? -1 : 0;
        clearImages();
        message("Textures: " + directory.u8string() + " (walls, floors and ceilings)");
    });
}
void App::launchMap() {
    action([&] {
        require(std::filesystem::is_regular_file(projectDirectory() / "Doom.exe"),
                "Cannot find Doom.exe in the project root");
        buildMap({}, true);
    });
}
void App::launchCompiledMap() {
    require(buildDestination.parent_path().parent_path() == projectDirectory() / "base",
            "Launch requires the compiled map in the project's base/maps directory");
    if (playCompiledMap) {
        playCompiledMap(buildMapName);
        return;
    }
    auto process =
        startProcess(projectDirectory() / "Doom.exe",
                     {L"+set", L"fs_basepath", projectDirectory().wstring(), L"+set", L"fs_game",
                      L"base", L"+map", std::wstring(buildMapName.begin(), buildMapName.end())},
                     projectDirectory(), buildDirectory / "game.log");
    CloseHandle(process);
    message("Launched Doom: " + buildMapName);
}
void App::buildMap(const std::filesystem::path &outputPath, bool launch) {
    action([&] {
        require(!buildProcess, "A map build is already running");
        navigationOnly = false;
        if (outputPath.empty()) {
            auto source = world.pathname;
            if (source.empty()) {
                source = fileDialog(true, L"DoomEd maps (*.dwd)\0*.dwd\0", L"dwd");
            }
            if (source.empty()) {
                return;
            }
            world.save(source);
            mapName = upper(source.stem().u8string());
            documentTitle = source.filename().u8string();
        }
        buildDestination =
            std::filesystem::absolute(outputPath.empty() ? projectDirectory() / "base/maps" /
                                                               (lowerName(mapName) + ".bsp")
                                                         : outputPath)
                .lexically_normal();
        require(upper(buildDestination.extension().u8string()) == ".BSP" &&
                    upper(buildDestination.parent_path().filename().u8string()) == "MAPS",
                "Compiled output must be <base directory>/maps/<mapname>.bsp");
        buildMapName = lowerName(buildDestination.stem().u8string());
        require(!buildMapName.empty() && buildMapName.size() <= 32 &&
                    buildMapName.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") ==
                        std::string::npos,
                "Map names must contain 1..32 letters, digits or underscores");
        const auto errors = world.validate(false);
        require(errors.empty(), errors.empty() ? "" : errors.front());
        buildWad = project.wadpath;
        require(std::filesystem::is_regular_file(buildWad), "Load a resource WAD before compiling");
        buildTextureDirectory = projectDirectory() / "base/textures";
        buildDirectory = projectDirectory() / "out/editor-builds" /
                         (buildMapName + "-" + std::to_string(GetCurrentProcessId()) + "-" +
                          std::to_string(GetTickCount64()));
        std::filesystem::create_directories(buildDirectory / "base/maps");
        writeTextAtomic(buildDirectory / (buildMapName + ".dwd"), world.serializeForCompiler());
        buildStage = 0;
        launchAfterBuild = launch;
        buildLog.clear();
        startBuildStage();
        showNativePanel("TextLog:Panel");
    });
}
void App::buildNavigation(const std::filesystem::path &bsp) {
    action([&] {
        require(!buildProcess, "A map build is already running");
        const auto bytes = readFile(bsp);
        const auto *invalid = dbsp::validate(bytes.data(), static_cast<int>(bytes.size()));
        require(!invalid, std::string("Invalid compiled BSP: ") + (invalid ? invalid : ""));
        buildDestination = std::filesystem::absolute(bsp).lexically_normal();
        buildMapName = lowerName(bsp.stem().u8string());
        require(!buildMapName.empty() && buildMapName.size() <= 32 &&
                    buildMapName.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") ==
                        std::string::npos,
                "Map names must contain 1..32 letters, digits or underscores");
        buildDirectory = projectDirectory() / "out/editor-builds" /
                         (buildMapName + "-aas-" + std::to_string(GetCurrentProcessId()) + "-" +
                          std::to_string(GetTickCount64()));
        std::filesystem::create_directories(buildDirectory / "base/maps");
        writeFileAtomic(buildDirectory / "base/maps" / bsp.filename(), bytes);
        navigationOnly = true;
        launchAfterBuild = false;
        buildStage = 1;
        buildLog.clear();
        startBuildStage();
    });
}
void App::startBuildStage() {
    const auto base = buildDirectory / "base";
    const auto bsp = base / "maps" / (buildMapName + ".bsp");
    std::vector<std::string> arguments;
    if (buildStage == 0) {
        arguments = {"dmap",
                     "--base",
                     base.string(),
                     "--wad",
                     buildWad.string(),
                     "--textures",
                     buildTextureDirectory.string(),
                     (buildDirectory / (buildMapName + ".dwd")).string()};
        message("Compiling " + buildMapName + " with dmap...");
    } else {
        const char *profiles[] = {"small", "medium", "cyber", "spider"};
        require(buildStage >= 1 && buildStage <= 4, "Invalid map build stage");
        const auto profile = profiles[buildStage - 1];
        arguments = {"aasbuild",  "--profile", profile, "--output", (base / "maps").string(),
                     bsp.string()};
        message("aasbuild navigation " + std::to_string(buildStage) + "/4: " + profile);
    }
    buildProcess = startCompilerJob(projectDirectory(),
                                    buildDirectory / ("stage-" + std::to_string(buildStage)),
                                    buildDirectory / "build.log", std::move(arguments));
}
void App::pollBuild() {
    if (!buildProcess) {
        refreshMapTextures();
        return;
    }
    {
        // Close the reader before a completed job releases its module and log.
        std::ifstream progress(buildDirectory / "build.log", std::ios::binary);
        if (progress) {
            buildLog.assign(std::istreambuf_iterator<char>(progress),
                            std::istreambuf_iterator<char>());
        }
    }
    auto *job = static_cast<CompilerJob *>(buildProcess);
    if (!compilerJobFinished(job)) {
        return;
    }
    const auto exitCode = finishCompilerJob(job);
    buildProcess = nullptr;
    action([&] {
        require(exitCode == 0, "Map compilation failed at stage " + std::to_string(buildStage) +
                                   " (exit " + std::to_string(exitCode) +
                                   "). Log: " + (buildDirectory / "build.log").u8string());
        const auto base = buildDirectory / "base";
        const auto bspPath = base / "maps" / (buildMapName + ".bsp");
        const auto bsp = readFile(bspPath);
        const auto *invalid = dbsp::validate(bsp.data(), static_cast<int>(bsp.size()));
        require(!invalid, std::string("Invalid compiled BSP: ") + (invalid ? invalid : ""));
        if (buildStage < 4) {
            ++buildStage;
            startBuildStage();
            return;
        }
        const auto checksum = bspChecksum(bsp);
        for (int worldIndex = 0; worldIndex < 4; ++worldIndex) {
            validateAas(base / "maps" / (buildMapName + "_b" + std::to_string(worldIndex) + ".aas"),
                        checksum);
        }
        const auto destinationBase = buildDestination.parent_path().parent_path();
        std::vector<std::pair<std::filesystem::path, std::optional<Bytes>>> previous;
        try {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(base)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                const auto relative = entry.path().lexically_relative(base);
                if (navigationOnly && relative.extension() != ".aas") {
                    continue;
                }
                const auto destination = destinationBase / relative;
                // Existing artist PNGs remain byte-for-byte intact.
                if (*relative.begin() == "textures" && std::filesystem::exists(destination)) {
                    continue;
                }
                std::filesystem::create_directories(destination.parent_path());
                previous.emplace_back(destination, std::filesystem::exists(destination)
                                                       ? std::optional<Bytes>(readFile(destination))
                                                       : std::nullopt);
                writeFileAtomic(destination, readFile(entry.path()));
            }
        } catch (...) {
            for (auto it = previous.rbegin(); it != previous.rend(); ++it) {
                if (it->second) {
                    writeFileAtomic(it->first, *it->second);
                } else {
                    std::filesystem::remove(it->first);
                }
            }
            throw;
        }
        const bool launch = launchAfterBuild;
        launchAfterBuild = false;
        project.wad.textureDirectory.clear();
        message("Built BSP and all four AAS profiles: " + buildDestination.u8string() +
                ". Log: " + (buildDirectory / "build.log").u8string());
        if (launch) {
            launchCompiledMap();
        }
    });
}
} // namespace de
