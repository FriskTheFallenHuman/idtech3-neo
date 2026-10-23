// SPDX-License-Identifier: GPL-3.0-or-later
#include "g_save_callbacks.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

// Constant initialization makes registration independent of the order in which
// translation units initialize. Constructors only link records; they allocate
// no memory and never call game/engine services while the DLL is being loaded.
doomSaveCallbackRegistration_t *pendingCallbacks = nullptr;
std::size_t registrationGeneration = 0;

struct callbackRegistry_t {
    std::vector<doomSaveCallback_t> entries;
    std::string error;
    std::size_t generation = static_cast<std::size_t>(-1);
};

callbackRegistry_t &Registry() {
    // Lookup can safely occur even from another translation unit's initializer.
    // Later registrations invalidate the cache, so early lookup cannot omit them.
    static callbackRegistry_t registry;
    if (registry.generation == registrationGeneration) {
        return registry;
    }
    registry.generation = registrationGeneration;
    registry.entries.clear();
    registry.error.clear();
    std::unordered_set<std::string> names;
    for (auto *record = pendingCallbacks; record; record = record->next) {
        const char *name = record->callback.funcStr;
        const char *reason = nullptr;
        if (!name || !*name) {
            reason = "empty save name";
        } else if (std::strlen(name) >= 512) {
            reason = "save name exceeds the legacy loader limit";
        } else if (!record->callback.funcPtr) {
            reason = "null function";
        } else if (!names.emplace(name).second) {
            reason = "duplicate save name";
        }
        if (reason) {
            char message[1024];
            std::snprintf(message, sizeof(message), "%s for '%s' at %s:%d", reason,
                          name ? name : "(null)", record->sourceFile, record->sourceLine);
            registry.error = message;
            registry.entries.clear();
            return registry;
        }
        registry.entries.push_back(record->callback);
    }
    // Folded/aliased functions use the alphabetically first registered name when
    // saving. Every registered alias remains valid when loading existing saves.
    // Startup/link order therefore never selects a callback's saved identity.
    std::sort(registry.entries.begin(), registry.entries.end(),
              [](const doomSaveCallback_t &a, const doomSaveCallback_t &b) {
                  return std::strcmp(a.funcStr, b.funcStr) < 0;
              });
    return registry;
}

} // namespace

doomSaveCallbackRegistration_t::doomSaveCallbackRegistration_t(const char *name,
                                                               doomSaveCallbackFunction_t function,
                                                               const char *file, int line) noexcept
    : callback{name, function}, sourceFile(file), sourceLine(line), next(pendingCallbacks) {
    pendingCallbacks = this;
    ++registrationGeneration;
}

bool DOOM_ValidateSaveCallbacks(const char **error) {
    auto &registry = Registry();
    if (!registry.error.empty()) {
        if (error) {
            *error = registry.error.c_str();
        }
        return false;
    }
    if (error) {
        *error = nullptr;
    }
    return true;
}

std::size_t DOOM_SaveCallbackCount() {
    return Registry().entries.size();
}

const doomSaveCallback_t *DOOM_FindSaveCallbackAtAddress(doomSaveCallbackFunction_t function) {
    if (!function) {
        return nullptr;
    }
    for (const auto &entry : Registry().entries) {
        if (entry.funcPtr == function) {
            return &entry;
        }
    }
    return nullptr;
}

doomSaveCallbackFunction_t DOOM_FindSaveCallbackByName(const char *name) {
    if (!name || !*name) {
        return nullptr;
    }
    for (const auto &entry : Registry().entries) {
        if (!std::strcmp(entry.funcStr, name)) {
            return entry.funcPtr;
        }
    }
    return nullptr;
}
