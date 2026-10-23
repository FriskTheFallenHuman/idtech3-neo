// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef G_SAVE_CALLBACKS_H
#define G_SAVE_CALLBACKS_H

// Put DOOM_SAVE_CALLBACK("permanentSaveName", Function) at namespace scope,
// immediately after the function's implementation. Keep the quoted save name
// when renaming the C++ function. Registrations exist before game initialization;
// saving/loading never depends on a callback having executed or an entity having
// spawned. Shared source files register only when compiled into the game DLL.
#if defined(DOOM_SAVE_CALLBACKS) || defined(DOOM_SAVE_CALLBACK_TEST)

#include <cstddef>
#include <type_traits>

using doomSaveCallbackFunction_t = void (*)();

struct doomSaveCallback_t {
    const char *funcStr;
    doomSaveCallbackFunction_t funcPtr;
};

struct doomSaveCallbackRegistration_t {
    doomSaveCallback_t callback;
    const char *sourceFile;
    int sourceLine;
    doomSaveCallbackRegistration_t *next;

    doomSaveCallbackRegistration_t(const char *name, doomSaveCallbackFunction_t function,
                                   const char *file, int line) noexcept;
    doomSaveCallbackRegistration_t(const doomSaveCallbackRegistration_t &) = delete;
    doomSaveCallbackRegistration_t &operator=(const doomSaveCallbackRegistration_t &) = delete;
};

template <typename Function>
doomSaveCallbackFunction_t DOOM_SaveCallbackAddress(Function function) noexcept {
    static_assert(std::is_pointer<Function>::value &&
                      std::is_function<typename std::remove_pointer<Function>::type>::value,
                  "DOOM_SAVE_CALLBACK requires a free-function pointer");
    // Erase the signature for identity lookup only. This pointer is never invoked;
    // the legacy loader restores the original entity callback's function type.
    return reinterpret_cast<doomSaveCallbackFunction_t>(function);
}

bool DOOM_ValidateSaveCallbacks(const char **error);
std::size_t DOOM_SaveCallbackCount();
const doomSaveCallback_t *DOOM_FindSaveCallbackAtAddress(doomSaveCallbackFunction_t function);
doomSaveCallbackFunction_t DOOM_FindSaveCallbackByName(const char *name);

#define DOOM_SAVE_CALLBACK_JOIN_INNER(a, b) a##b
#define DOOM_SAVE_CALLBACK_JOIN(a, b) DOOM_SAVE_CALLBACK_JOIN_INNER(a, b)
#define DOOM_SAVE_CALLBACK(saveName, function)                                                     \
    namespace {                                                                                    \
    static doomSaveCallbackRegistration_t                                                          \
        DOOM_SAVE_CALLBACK_JOIN(doom_save_callback_,                                               \
                                __COUNTER__)(saveName, DOOM_SaveCallbackAddress(function),         \
                                             __FILE__, __LINE__);                                  \
    }

#else
#define DOOM_SAVE_CALLBACK(saveName, function)
#endif

#endif
