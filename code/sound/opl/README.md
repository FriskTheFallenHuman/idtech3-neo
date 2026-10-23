# DOS AdLib music

`doom_opl.cpp` adapts the instrument/voice portions of Chocolate Doom's
`src/i_oplmusic.c`, preserving the original copyright and GPL notices:
https://github.com/chocolate-doom/chocolate-doom/blob/master/src/i_oplmusic.c

The adapter uses Doom 1.9 DMX behavior, nine OPL2 voices, GENMIDI packed 36-byte
instrument records, original frequency/volume tables and channel priority.
Engine-independent C++ MIDI parsing and sample scheduling live in `../snd_opl.cpp`.
No SDL, hardware OPL access, external timers or audio callbacks are used here.

`opl3.c`, `opl3.h` and `LICENSE` are unmodified Nuked OPL3 sources pinned to
commit 765ec962e473aeb767e4cba74ffdc8f588ffbfe8:
https://github.com/nukeykt/Nuked-OPL3

Nuked runs in OPL2 compatibility mode at a 44100-Hz output rate. Buffered register
writes preserve chip write timing. The GENMIDI asset is extracted from the user's
installed WAD, never downloaded or replaced with a General MIDI sound bank.
