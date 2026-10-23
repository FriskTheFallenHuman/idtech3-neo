// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/g_save_callbacks.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "FAILED %d: %s\n", __LINE__, #condition);                         \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (0)

namespace {
int calls;
void LegacyFunction() {
    ++calls;
}
void NewFunction() {
    calls += 2;
}
int TypedFunction(int value) {
    return value + 7;
}
void UnregisteredFunction() {
    calls += 3;
}

// Exercise lookup before registrations in this translation unit initialize.
struct EarlyLookup {
    EarlyLookup() { DOOM_FindSaveCallbackByName("typed_callback"); }
} earlyLookup;

// Deliberately reverse lexical order to simulate different link orders.
DOOM_SAVE_CALLBACK("DOOM_MoverUse", LegacyFunction);
DOOM_SAVE_CALLBACK("DoomMonsterThink", LegacyFunction);
DOOM_SAVE_CALLBACK("A0_new_alias", LegacyFunction);
DOOM_SAVE_CALLBACK("zz_new_alias", NewFunction);
DOOM_SAVE_CALLBACK("aa_new_alias", NewFunction);
DOOM_SAVE_CALLBACK("typed_callback", TypedFunction);
} // namespace

int DOOM_TestOtherTranslationUnit(int value);

int main(int argc, char **argv) {
    const char *error = nullptr;
    CHECK(DOOM_ValidateSaveCallbacks(&error));
    CHECK(error == nullptr);
    CHECK(DOOM_SaveCallbackCount() == 7);
    CHECK(DOOM_FindSaveCallbackByName("missing_callback") == nullptr);
    CHECK(DOOM_FindSaveCallbackByName(nullptr) == nullptr);
    CHECK(DOOM_FindSaveCallbackAtAddress(nullptr) == nullptr);
    CHECK(DOOM_FindSaveCall
backAtAddress(DOOM_SaveCallbackAddress(UnregisteredFunction)) ==
          nullptr);
    const auto *legacy = DOOM_FindSaveCallbackAtAddress(DOOM_SaveCallbackAddress(LegacyFunction));
    CHECK(legacy && !std::strcmp(legacy->funcStr, "A0_new_alias"));
    CHECK(DOOM_FindSaveCallbackByName("DOOM_MoverUse") == legacy->funcPtr);
    CHECK(DOOM_FindSaveCallbackByName("DoomMonsterThink") == legacy->funcPtr);
    DOOM_FindSaveCallbackByName("DOOM_MoverUse")();
    CHECK(calls == 1);
    const auto *recent = DOOM_FindSaveCallbackAtAddress(DOOM_SaveCallbackAddress(NewFunction));
    CHECK(recent && !std::strcmp(recent->funcStr, "aa_new_alias"));
    auto typed = reinterpret_cast<int (*)(int)>(DOOM_FindSaveCallbackByName("typed_callback"));
    CHECK(typed && typed(5) == 12);
    auto other = reinterpret_cast<int (*)(int)>(DOOM_FindSaveCallbackByName("other_file_callback"));
    CHECK(other && other(4) == DOOM_TestOtherTranslationUnit(4));

    if (argc > 1) {
        const char *name = "typed_callback";
        auto function = DOOM_SaveCallbackAddress(NewFunction);
        const char *expected = "duplicate save name";
        char longName[513];
        if (!std::strcmp(argv[1], "empty")) {
            name = "";
            expected = "empty save name";
        } else if (!std::strcmp(argv[1], "null")) {
            name = "null_callback";
            function = nullptr;
            expected = "null function";
        } else if (!std::strcmp(argv[1], "long")) {
            std::memset(longName, 'x', sizeof(longName) - 1);
            longName[sizeof(longName) - 1] = 0;
            name = longName;
            expected = "loader limit";
        }
        // Cache invalidation also protects against lookup during static initialization.
        doomSaveCallbackRegistration_t invalid(name, function, __FILE__, __LINE__);
        CHECK(!DOOM_ValidateSaveCallbacks(&error));
        CHECK(error && std::strstr(error, expected));
        CHECK(std::strstr(error, "doom_save_callbacks_
tests.cpp"));
        CHECK(DOOM_FindSaveCallbackByName("typed_callback") == nullptr);
    }
    std::puts("Save callback registration checks passed.");
    return 0;
}
