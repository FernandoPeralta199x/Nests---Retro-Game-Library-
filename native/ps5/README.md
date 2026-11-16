# Native PS5 shell source

This directory contains the initial NESTS native SDL2 shell. The six items are fictional UI fixtures. The shell does not fetch catalogs or download ROMs.

## Build using the reference native workspace

See docs/PS5_NATIVE_INTEGRATION.md at repository root. In summary:

1. Clone the PS5 native app boilerplate.
2. Keep its build scripts, linker and runtime files intact.
3. Replace its src/main.cpp with this main.cpp.
4. Configure a unique PPSA ID.
5. Run make PACBREW_PACKAGES=sdl2 app.
6. After inspecting the folder output, run make PACBREW_PACKAGES=sdl2 ffpfsc.

This build recipe is a development target only. It has not been tested against firmware 13.60.

## Input mapping

SDL controller events are used instead of hand-written Sony pad structure layouts:

- D-pad: move selection
- A / expected Cross mapping: add or remove favorite
- B / expected Circle mapping: clear favorites-only view
- Start / Options mapping: toggle favorites-only view

Controller glyph mapping can differ by SDL controller database and firmware. Confirm the actual DualSense mapping on hardware.

Keyboard fallback for host testing: arrow keys, Enter or Space, F to toggle favorites view, Esc to return to all games.

## Persistence

Favorites are saved in /download0/nests/favorites.txt using a temporary file, fflush, fsync and rename. Only IDs present in the built-in demo catalog are accepted. This initial store is intentionally small and bounded by the six fixture IDs.

## Not implemented in this shell

Network catalog updates, TLS, Ed25519 verification, native download transport, streaming SHA-256, archive extraction, ROM scanning, emulator configuration and emulator launching.


## Host syntax check

GitHub Actions runs a host-side SDL2 C++ syntax check for this file. This checks that the source is accepted by the host compiler and installed SDL2 headers; it is not a PS5 cross-compilation and does not prove firmware 13.60 compatibility.
