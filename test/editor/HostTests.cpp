// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorAPI.h"
#include "SectorAuthoring.h"
#include "idfunctions.h"
#include <SDL3/SDL.h>
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
    PROCESSENTRY32 process{};
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
std::string windowTitle(SDL_Window *window) {
    return SDL_GetWindowTitle(window);
}
void drawFrames(const DoomEditorAPI &api, int count = 3) {
    for (int frame = 0; frame < count; ++frame) {
        api.draw(1440, 900);
    }
}
// The editor consumes SDL events, so the test feeds it the same events a real
// window would receive.
SDL_Event keyEvent(SDL_Window *window, SDL_Scancode scancode, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = SDL_GetWindowID(window);
    event.key.scancode = scancode;
    event.key.key = SDL_SCANCODE_TO_KEYCODE(scancode);
    event.key.down = down;
    return event;
}
SDL_Event textEvent(SDL_Window *window, const char *text) {
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = SDL_GetWindowID(window);
    event.text.text = text;
    return event;
}
SDL_Event motionEvent(SDL_Window *window, float x, float y) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.x = x;
    event.motion.y = y;
    return event;
}
SDL_Event buttonEvent(SDL_Window *window, float x, float y, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = 1;
    event.button.down = down;
    event.button.clicks = 1;
    event.button.x = x;
    event.button.y = y;
    return event;
}
void pressKey(const DoomEditorAPI &api, SDL_Window *window, SDL_Scancode scancode) {
    SDL_Event event = keyEvent(window, scancode, true);
    api.sdlevent(&event);
    drawFrames(api);
    event = keyEvent(window, scancode, false);
    api.sdlevent(&event);
    drawFrames(api);
}
void clickAt(const DoomEditorAPI &api, SDL_Window *window, int x, int y) {
    const float fx = static_cast<float>(x), fy = static_cast<float>(y);
    SDL_Event event = motionEvent(window, fx, fy);
    api.sdlevent(&event);
    drawFrames(api);
    event = buttonEvent(window, fx, fy, true);
    api.sdlevent(&event);
    drawFrames(api);
    event = buttonEvent(window, fx, fy, false);
    api.sdlevent(&event);
    drawFrames(api);
}
void chooseObject(const DoomEditorAPI &api, SDL_Window *window, const char *name) {
    pressKey(api, window, SDL_SCANCODE_F4);
    SDL_Event event = textEvent(window, name);
    api.sdlevent(&event);
    drawFrames(api);
    pressKey(api, window, SDL_SCANCODE_RETURN);
}
} // namespace

int main(int argc, char **argv) {
    HMODULE module = nullptr;
    const DoomEditorAPI *api = nullptr;
    SDL_Window *window = nullptr;
    try {
        de::require(argc == 5, "Expected editor DLL, scratch project, WAD, and tools directory");
        if (!std::filesystem::exists(argv[3])) {
            // CI has no game data; the host suite needs the retail WAD.
            std::cout << "Editor host tests skipped: " << argv[3] << " is not available\n";
            return 0;
        }
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
        de::require(bool(retainedLog), "Cannot create host CRT stream");
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
        de::require(SDL_Init(SDL_INIT_VIDEO), "Cannot initialize SDL video for the host test");
        window = SDL_CreateWindow("Editor host test", 1440, 900, SDL_WINDOW_RESIZABLE);
        de::require(window != nullptr, "Cannot create test input window");
        const auto rootText = root.u8string();
        DoomEditorHost host{DoomEditorAPIVersion,
                            sizeof(DoomEditorHost),
                            window,
                            nullptr,
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
        de::require(api->show(""), api->lastError());
        de::require(windowTitle(window) == "DoomEdit: Untitled.dwd",
                    "New map did not set the untitled editor caption");
        de::require((SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) != 0,
                    "Editor lacks native minimize/maximize/resize controls");
        int minWidth = 0, minHeight = 0;
        SDL_GetWindowMinimumSize(window, &minWidth, &minHeight);
        de::require(minWidth >= 960 && minHeight >= 640,
                    "Editor did not enforce a usable minimum size");
        SDL_MaximizeWindow(window);
        de::require((SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0,
                    "Native maximize control did not maximize the editor");
        SDL_MinimizeWindow(window);
        de::require((SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0 && api->visible(),
                    "Native minimize control did not retain the editor document");
        SDL_RestoreWindow(window);
        de::require((SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) == 0 &&
                        (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0,
                    "Restoring a minimized maximized editor lost maximization");
        SDL_RestoreWindow(window);
        de::require((SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) == 0,
                    "Native restore control did not unmaximize the editor");
        SDL_HideWindow(window);
        de::require(api->show("worktest"), api->lastError());
        de::require(windowTitle(window) ==
                        "DoomEdit: " + std::filesystem::path(source).make_preferred().u8string(),
                    "Editor caption did not include the full DWD path");
        for (int i = 0; i < 4; ++i) {
            api->draw(1440, 900);
        }
        de::require(api->visible() && uiFrames >= 4 && previews >= 4, messages);
        chooseObject(*api, window, "Imp");
        clickAt(*api, window, 600, 450);
        chooseObject(*api, window, "Shotgun");
        clickAt(*api, window, 700, 450);
        clickAt(*api, window, 85, 49); // Save the existing DWD through the toolbar.
        de::EditWorld placed;
        placed.load(source);
        de::require(placed.things.size() == 3 && placed.things[1].type == 3001 &&
                        placed.things[2].type == 2001 && placed.things[0].type == 1,
                    "Object catalog search/selection did not place the chosen object types");
        original = de::readFile(source);
        pressKey(*api, window, SDL_SCANCODE_F3);
        SDL_Event keyPress = keyEvent(window, SDL_SCANCODE_F2, true);
        api->sdlevent(&keyPress);
        for (int i = 0; i < 3; ++i) {
            api->draw(1440, 900);
        }
        const auto planPreviews = previews;
        api->draw(1440, 900);
        de::require(previews == planPreviews, "Plan tab still rendered a perspective viewport");
        keyPress = keyEvent(window, SDL_SCANCODE_F2, false);
        api->sdlevent(&keyPress);
        keyPress = keyEvent(window, SDL_SCANCODE_F3, true);
        api->sdlevent(&keyPress);
        for (int i = 0; i < 3; ++i) {
            api->draw(1440, 900);
        }
        de::require(previews > planPreviews, "Perspective tab did not restore the scene");
        keyPress = keyEvent(window, SDL_SCANCODE_F3, false);
        api->sdlevent(&keyPress);
        const auto previousFloors = floorVertices, previousCeilings = ceilingVertices;
        de::require(api->show("e1m2"), api->lastError());
        drawFrames(*api);
        de::require(floorVertices > previousFloors && ceilingVertices > previousCeilings,
                    "E1M2 preview omitted floor/ceiling geometry\n" + messages);
        SDL_MaximizeWindow(window);
        SDL_HideWindow(window);
        int maximizedX = 0, maximizedY = 0, maximizedWidth = 0, maximizedHeight = 0;
        SDL_GetWindowPosition(window, &maximizedX, &maximizedY);
        SDL_GetWindowSize(window, &maximizedWidth, &maximizedHeight);
        keyPress = keyEvent(window, SDL_SCANCODE_F12, true);
        api->sdlevent(&keyPress);
        api->draw(1440, 900);
        api->frame();
        de::require(!api->visible() && returned == 1, "F12 did not return to game");
        de::require(windowTitle(window) == "Editor host test",
                    "Returning to game did not restore the game caption");
        int restoredX = 0, restoredY = 0, restoredWidth = 0, restoredHeight = 0;
        SDL_GetWindowPosition(window, &restoredX, &restoredY);
        SDL_GetWindowSize(window, &restoredWidth, &restoredHeight);
        de::require((SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0 &&
                        maximizedX == restoredX && maximizedY == restoredY &&
                        maximizedWidth == restoredWidth && maximizedHeight == restoredHeight,
                    "Returning to game lost the maximized window state");
        de::require(api->show(""), "Cannot reopen editor document");
        de::require((SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0,
                    "Reopening editor lost the maximized window state");
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
        memoryBefore.cb = sizeof(memoryBefore);
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
        de::require(api->build("worktest", true), api->lastError());
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
        SDL_DestroyWindow(window);
        window = nullptr;
        SDL_Quit();
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
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
        return 1;
    }
}
