# Native PS5 integration — initial shell milestone

## Current status

The native shell source in native/ps5 is an SDL2-based proof of concept. It is separate from the browser preview. It demonstrates a TV-sized game catalog, D-pad navigation, confirm/back actions and favorites persisted under the title-local /download0/nests/ directory.

This source has not yet been built with the PS5 cross-toolchain or executed on firmware 13.60. A host-side SDL build is not proof of console compatibility.

## Build environment

Use the public ps5-native-app-boilerplate as the build workspace. It provides the PS5 native container, toolchain bootstrap, linker and package assembly scripts. Do not copy Pegasus DL+ source or assets.

1. Clone https://github.com/drakmor/ps5-native-app-boilerplate.
2. Install its documented Ubuntu/WSL prerequisites.
3. Copy native/ps5/main.cpp from this repository over the boilerplate's src/main.cpp.
4. Configure a unique PPSA title ID using the boilerplate's make init target.
5. Build with PacBrew SDL2 enabled: make PACBREW_PACKAGES=sdl2 app.
6. Build the compressed package only after the folder build succeeds: make PACBREW_PACKAGES=sdl2 ffpfsc.
7. Inspect the package contents and digest before moving it to the console.

The boilerplate documents the SDL2 PacBrew route, but the SDL2 module and its runtime imports still require validation on the target firmware and loader combination.

## Input

The shell uses SDL_GameController events rather than hard-coded controller structure offsets. The expected mapping is D-pad for navigation, A/Cross for select, B/Circle for back, and Start for options. Validate the actual DualSense mapping on the target build; do not assume it is correct from desktop testing.

## Storage

Favorites are written only to /download0/nests/favorites.txt. The write path stages a temporary file, flushes it, fsyncs it, and renames it atomically. The storage is title-local and is not a substitute for an exportable backup.

## Deliberately not included

- Network transport and catalog refresh in the native shell.
- Native TLS certificate validation.
- Native Ed25519 signature verification.
- Streaming download and SHA-256 verification.
- Archive extraction.
- Emulator discovery or launching.

These features must not be enabled until each platform service has a reviewed implementation and firmware-specific tests.


## File storage preference

The native shell creates `/download0/nests/roms/` when the title starts and exposes a Settings view for file organization: all ROMs in one folder or separated into console folders. The choice is saved atomically in `/download0/nests/settings.txt`.

The download root is intentionally restricted to the title-local writable area documented by the native template. External USB storage is not exposed until mount discovery, write permissions and removal/reconnect behavior have been validated on the target firmware. The downloader itself is not implemented yet; the setting is groundwork for its destination.
