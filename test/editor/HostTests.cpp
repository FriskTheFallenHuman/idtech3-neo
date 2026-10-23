// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorAPI.h"
#include "SectorAuthoring.h"
#include "idfunctions.h"
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
std::string messages;
uint64_t nextTexture = 1;
unsigned uiFrames = 0, previews = 0, returned = 0;
unsigned floorVertices = 0, ceilingVertices = 0;
void logMessage(const char *text) {
    messages += std::string(text) + '\n';
}
void returnToGame() {
    ++returned;
}
void playMap(const char *) {
}
uint64_t createTexture(int width, int height, const uint32_t *) {
    return width > 0 && height > 0 ? nextTexture++ : 0;
}
void destroyTexture(uint64_t) {
}
uint64_t preview(const EditorPreviewScene *scene) {
    de::require(scene && scene->vertexCount > 0 && scene->batchCount > 0,
                "Preview did not submit editable geometry");
    ++previews;
    for (uint32_t index = 0; index + 2 < scene->vertexCount; index += 3) {
        const auto *a = scene->vertices[index].position;
        const auto *b = scene->vertices[index + 1].position;
        const auto *c = scene->vertices[index + 2].position;
        const float area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
        if (std::abs(area) > .001f && a[2] == b[2] && a[2] == c[2]) {
            floorVertices += a[2] == 24;
            ceilingVertices += a[2] == 96;
        }
    }
    return 999999;
}
bool drawUI(const EditorUIDraw *draw) {
    if (!draw || draw->vertexCount < 100 || draw->commandCount < 5) {
        return false;
    }
    for (uint32_t i = 0; i < draw->indexCount; ++i) {
        if (draw->indices[i] >= draw->vertexCount) {
            return false;
        }
    }
    ++uiFrames;
    return true;
}
void waitForBuild(const DoomEditorAPI &api) {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    de::require(snapshot != INVALID_HANDLE_VALUE, "Cannot inspect compiler process isolation");
    PROCESSE
NTRY32 process{};
    process.dwSize = sizeof(process);
    bool child = false;
    if (Process32First(snapshot, &process)) {
        do {
            child |= process.th32ParentProcessID == GetCurrentProcessId();
        } while (Process32Next(snapshot, &process));
    }
    CloseHandle(snapshot);
    de::require(!child, "Integrated build created a child process");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    while (messages.find("Built BSP and all four AAS profiles:") == std::string::npos) {
        de::require(std::chrono::steady_clock::now() < deadline, "Compiler pipeline timed out");
        de::require(messages.find("Error:") == std::string::npos, messages);
        api.frame();
        Sleep(10);
    }
}
std::wstring windowTitle(HWND window) {
    wchar_t title[32768]{};
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    return title;
}
void drawFrames(const DoomEditorAPI &api, int count = 3) {
    for (int frame = 0; frame < count; ++frame) {
        api.draw(1440, 900);
    }
}
void pressKey(const DoomEditorAPI &api, HWND window, int key) {
    api.windowMessage(window, WM_KEYDOWN, key,
                      static_cast<intptr_t>(MapVirtualKeyW(key, MAPVK_VK_TO_VSC)) << 16);
    drawFrames(api);
    api.windowMessage(window, WM_KEYUP, key, 0);
    drawFrames(api);
}
void clickAt(const DoomEditorAPI &api, HWND window, int x, int y) {
    api.windowMessage(window, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
    drawFrames(api);
    api.windowMessage(window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
    drawFrames(api);
    api.windowMessage(window, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    drawFrames(api);
}
void chooseObject(const DoomEditorAPI &api, HWND window, const char *name) {
    pressKey(api, window, VK_F4);
    for (const auto *letter = name; *letter; ++letter) {
        api.windowMessage(window, WM_CHAR, static_cast<unsigned char>(*letter), 0);
    }
    drawFrames(api);
    pressKey
(api, window, VK_RETURN);
}
} // namespace

int main(int argc, char **argv) {
    HMODULE module = nullptr;
    const DoomEditorAPI *api = nullptr;
    HWND window = nullptr;
    try {
        de::require(argc == 5, "Expected editor DLL, scratch project, WAD, and tools directory");
        const auto root = std::filesystem::absolute(argv[2]);
        const auto tools = std::filesystem::absolute(argv[4]);
        std::filesystem::create_directories(root / "base/maps");
        std::filesystem::create_directories(root / "base/wad");
        std::filesystem::create_directories(root / "base/aas");
        std::filesystem::copy_file(argv[3], root / "base/wad/DOOM.WAD",
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(tools / "base/maps/e1m2.dwd", root / "base/maps/e1m2.dwd",
                                   std::filesystem::copy_options::overwrite_existing);
        for (const auto *profile : {"small", "medium", "cyber", "spider"}) {
            const auto name = std::string("doom_") + profile + ".cfg";
            std::filesystem::copy_file(tools / "base/aas" / name, root / "base/aas" / name,
                                       std::filesystem::copy_options::overwrite_existing);
        }
        de::EditWorld world;
        world.clear();
        de::Line prototype;
        prototype.side[0].midtexture = "FLOOR0_1";
        prototype.side[0].ends.floorflat = "STARTAN3";
        prototype.side[0].ends.ceilingflat = "STARTAN3";
        prototype.side[0].ends.ceilingheight = 256;
        de::authorSector(world, {{0, 0}, {512, 0}, {512, 512}, {0, 512}}, prototype);
        world.things.push_back({{256, 256}, 0, 1, 7});
        const auto source = root / "base/maps/worktest.dwd";
        world.save(source);
        auto original = de::readFile(source);
        const auto retainedLogPath = root / "host-stream.txt";
        std::ofstream retainedLog(retainedLogPath, std::ios::binary);
        de::re
quire(bool(retainedLog), "Cannot create host CRT stream");
        module = LoadLibraryW(std::filesystem::absolute(argv[1]).c_str());
        de::require(module != nullptr, "Cannot load editor DLL");
        const auto *image = reinterpret_cast<const unsigned char *>(module);
        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(image);
        const auto *pe = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(image + dos->e_lfanew);
        de::require(pe->OptionalHeader.SizeOfImage < 32 * 1024 * 1024,
                    "Tools DLL reserves oversized legacy compiler tables at load time");
        auto getAPI =
            reinterpret_cast<DoomEditorGetAPI>(GetProcAddress(module, "DoomEditor_GetAPI"));
        de::require(getAPI && !getAPI(DoomEditorAPIVersion + 1), "API version mismatch accepted");
        api = getAPI(DoomEditorAPIVersion);
        de::require(api && api->size == sizeof(DoomEditorAPI), "Invalid editor API layout");
        window = CreateWindowExW(0, L"STATIC", L"Editor host test",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0, 0, 1440, 900, nullptr,
                                 nullptr, GetModuleHandleW(nullptr), nullptr);
        de::require(window != nullptr, "Cannot create test input window");
        const auto rootText = root.wstring();
        DoomEditorHost host{DoomEditorAPIVersion,
                            sizeof(DoomEditorHost),
                            window,
                            rootText.c_str(),
                            logMessage,
                            returnToGame,
                            playMap,
                            {createTexture, destroyTexture, preview, drawUI}};
        auto wrong = host;
        wrong.size = 0;
        de::require(!api->initialize(&wrong), "Host size mismatch accepted");
        de::require(api->initialize(&host), api->lastError());
        de::require(!api->show("../escape"), "Editor accepted path traversal");
        de::require
(api->show(""), api->lastError());
        de::require(windowTitle(window) == L"DoomEdit: Untitled.dwd",
                    "New map did not set the untitled editor caption");
        const auto editorStyle = GetWindowLongPtrW(window, GWL_STYLE);
        de::require((editorStyle & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW,
                    "Editor lacks native minimize/maximize/resize controls");
        MINMAXINFO limits{};
        de::require(
            api->windowMessage(window, WM_GETMINMAXINFO, 0, reinterpret_cast<intptr_t>(&limits)) &&
                limits.ptMinTrackSize.x >= 960 && limits.ptMinTrackSize.y >= 640,
            "Editor did not enforce a usable minimum size");
        SendMessageW(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        de::require(IsZoomed(window), "Native maximize control did not maximize the editor");
        SendMessageW(window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        de::require(IsIconic(window) && api->visible(),
                    "Native minimize control did not retain the editor document");
        SendMessageW(window, WM_SYSCOMMAND, SC_RESTORE, 0);
        de::require(!IsIconic(window) && IsZoomed(window),
                    "Restoring a minimized maximized editor lost maximization");
        SendMessageW(window, WM_SYSCOMMAND, SC_RESTORE, 0);
        de::require(!IsZoomed(window), "Native restore control did not unmaximize the editor");
        ShowWindow(window, SW_HIDE);
        de::require(api->show("worktest"), api->lastError());
        de::require(windowTitle(window) ==
                        L"DoomEdit: " + std::filesystem::path(source).make_preferred().wstring(),
                    "Editor caption did not include the full DWD path");
        for (int i = 0; i < 4; ++i) {
            api->draw(1440, 900);
        }
        de::require(api->visible() && uiFrames >= 4 && previews >= 4, messages);
        chooseObject(*api, window, "Imp");
        clickAt(*api, window, 600, 450);
        chooseObject(*api, win
dow, "Shotgun");
        clickAt(*api, window, 700, 450);
        clickAt(*api, window, 85, 49); // Save the existing DWD through the toolbar.
        de::EditWorld placed;
        placed.load(source);
        de::require(placed.things.size() == 3 && placed.things[1].type == 3001 &&
                        placed.things[2].type == 2001 && placed.things[0].type == 1,
                    "Object catalog search/selection did not place the chosen object types");
        original = de::readFile(source);
        pressKey(*api, window, VK_F3);
        api->windowMessage(window, WM_KEYDOWN, VK_F2,
                           static_cast<intptr_t>(MapVirtualKeyW(VK_F2, MAPVK_VK_TO_VSC)) << 16);
        for (int i = 0; i < 3; ++i) {
            api->draw(1440, 900);
        }
        const auto planPreviews = previews;
        api->draw(1440, 900);
        de::require(previews == planPreviews, "Plan tab still rendered a perspective viewport");
        api->windowMessage(window, WM_KEYUP, VK_F2, 0);
        api->windowMessage(window, WM_KEYDOWN, VK_F3,
                           static_cast<intptr_t>(MapVirtualKeyW(VK_F3, MAPVK_VK_TO_VSC)) << 16);
        for (int i = 0; i < 3; ++i) {
            api->draw(1440, 900);
        }
        de::require(previews > planPreviews, "Perspective tab did not restore the scene");
        api->windowMessage(window, WM_KEYUP, VK_F3, 0);
        const auto previousFloors = floorVertices, previousCeilings = ceilingVertices;
        de::require(api->show("e1m2"), api->lastError());
        drawFrames(*api);
        de::require(floorVertices > previousFloors && ceilingVertices > previousCeilings,
                    "E1M2 preview omitted floor/ceiling geometry\n" + messages);
        SendMessageW(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        ShowWindow(window, SW_HIDE);
        RECT maximizedRect{};
        GetWindowRect(window, &maximizedRect);
        api->windowMessage(window, WM_KEYDOWN, VK_F12,
                           static_cast<intptr
_t>(MapVirtualKeyW(VK_F12, MAPVK_VK_TO_VSC)) << 16);
        api->draw(1440, 900);
        api->frame();
        de::require(!api->visible() && returned == 1, "F12 did not return to game");
        de::require(windowTitle(window) == L"Editor host test",
                    "Returning to game did not restore the game caption");
        RECT restoredRect{};
        GetWindowRect(window, &restoredRect);
        de::require(IsZoomed(window) && EqualRect(&maximizedRect, &restoredRect),
                    "Returning to game lost the maximized window state");
        de::require(api->show(""), "Cannot reopen editor document");
        de::require(IsZoomed(window), "Reopening editor lost the maximized window state");
        api->resetGraphics();
        api->draw(1440, 900);
        de::require(api->visible(), "Graphics reset lost the editor document");
        de::require(api->show(""), "Cannot return to game via console toggle");
        messages.clear();
        de::require(api->build("worktest", false), api->lastError());
        de::require(!api->build("worktest", false), "Concurrent compiler pipeline was accepted");
        waitForBuild(*api);
        de::require(de::readFile(source) == original, "Compilation modified the DWD source");
        const auto bsp = root / "base/maps/worktest.bsp";
        const auto compiled = de::readFile(bsp);
        messages.clear();
        de::require(api->build("worktest", true), api->lastError());
        waitForBuild(*api);
        de::require(de::readFile(bsp) == compiled, "Navigation-only build modified BSP");
        for (int i = 0; i < 4; ++i) {
            const auto bytes =
                de::readFile(root / "base/maps" / ("worktest_b" + std::to_string(i) + ".aas"));
            de::require(bytes.size() > 124, "Missing navigation profile");
        }
        DWORD handlesBefore = 0;
        GetProcessHandleCount(GetCurrentProcess(), &handlesBefore);
        PROCESS_MEMORY_COUNTERS_EX memoryBefore{};
        memoryBefore.c
b = sizeof(memoryBefore);
        GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memoryBefore),
                             sizeof(memoryBefore));
        for (int repeat = 0; repeat < 5; ++repeat) {
            messages.clear();
            de::require(api->build("worktest", true), api->lastError());
            waitForBuild(*api);
        }
        DWORD handlesAfter = 0;
        GetProcessHandleCount(GetCurrentProcess(), &handlesAfter);
        PROCESS_MEMORY_COUNTERS_EX memoryAfter{};
        memoryAfter.cb = sizeof(memoryAfter);
        GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memoryAfter),
                             sizeof(memoryAfter));
        de::require(handlesAfter <= handlesBefore + 2, "Repeated compiler jobs leaked handles");
        de::require(memoryAfter.PrivateUsage <= memoryBefore.PrivateUsage + 16 * 1024 * 1024,
                    "Repeated compiler jobs retained their memory arenas");
        retainedLog << "Host CRT stream survived compiler jobs\n" << std::flush;
        de::require(bool(retainedLog), "Compiler closed the host CRT stream");
        const auto retainedBytes = de::readFile(retainedLogPath);
        de::require(std::string(retainedBytes.begin(), retainedBytes.end()) ==
                        "Host CRT stream survived compiler jobs\n",
                    "Compiler corrupted the host CRT stream");
        const auto profilePath = root / "base/aas/doom_small.cfg";
        const auto profile = de::readFile(profilePath);
        std::vector<de::Bytes> navigation;
        for (int index = 0; index < 4; ++index) {
            navigation.push_back(
                de::readFile(root / "base/maps" / ("worktest_b" + std::to_string(index) + ".aas")));
        }
        de::writeTextAtomic(profilePath, "invalid configuration syntax\n");
        messages.clear();
        de::require(api->build("wor
ktest", true), api->lastError());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (messages.find("Error:") == std::string::npos) {
            de::require(std::chrono::steady_clock::now() < deadline,
                        "Legacy compiler failure did not return to host");
            api->frame();
            Sleep(10);
        }
        de::writeFileAtomic(profilePath, profile);
        for (int index = 0; index < 4; ++index) {
            de::require(de::readFile(root / "base/maps" /
                                     ("worktest_b" + std::to_string(index) + ".aas")) ==
                            navigation[index],
                        "Failed navigation compile modified installed output");
        }
        messages.clear();
        de::require(api->build("worktest", true), api->lastError());
        // Shutdown while an AAS worker is live; it must unwind and join before
        // either the editor or the host unloads its DLL.
        api->shutdown();
        api = nullptr;
        FreeLibrary(module);
        module = nullptr;
        DestroyWindow(window);
        window = nullptr;
        std::cout << "Editor host: native min/max/restore, maximized game/editor transitions, "
                     "captions, object search/placement, version/layout rejection, "
                     "renderer submissions, F12, reopen, "
                     "graphics reset, async dmap/four AAS profiles, build exclusion and source "
                     "preservation, no child process, repeat memory/handle bounds and cancellation "
                     "passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n' << messages;
        if (api) {
            api->shutdown();
        }
        if (module) {
            FreeLibrary(module);
        }
        if (window) {
            DestroyWindow(window);
        }
        return 1;
    }
}
