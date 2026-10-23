// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/g_save_callbacks.h"

namespace {
// File-local callbacks need no exported symbol or central forward declaration.
int OtherFileCallback(int value) {
    return value * 3;
}
DOOM_SAVE_CALLBACK("other_file_callback", OtherFileCallback);
} // namespace

int DOOM_TestOtherTranslationUnit(int value) {
    return OtherFileCallback(value);
}
