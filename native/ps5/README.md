# NESTS native PS5 shell and download pipeline

This directory contains the SDL2 TV interface and the native storage/download services. The six visible catalog items remain fictional UI fixtures; none has an authorized download package attached.

## Build using the reference native workspace

Use the public [PS5 native app boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate) as the build workspace. Keep its build scripts, SDK, linker and runtime files intact.

Copy these source files into the template's `src/` directory:

- `main.cpp` (replace the template's `src/main.cpp`)
- `storage_manager.cpp` / `storage_manager.hpp`
- `sha256.cpp` / `sha256.hpp`
- `http_download.cpp` / `http_download.hpp`

Configure a unique PPSA title ID using the template's documented `make init` step. The NESTS shell uses SDL2 for rendering, while the download service calls the PS5 system `sceNet`, `sceSsl` and `sceHttp` APIs directly; it does not add libcurl or OpenSSL archives.

Build the application folder first:

```bash
make PACBREW_PACKAGES=sdl2 app
```

Only after inspecting that output, build the compressed package:

```bash
make PACBREW_PACKAGES=sdl2 ffpfsc
```

This recipe has not yet been run with the target cross-toolchain for NESTS, and no `.ffpfsc` from this branch has been tested on firmware 13.60.

## Storage settings and USB

The default internal destination is:

```text
/download0/nests/roms/
```

At launch the native app scans the title-local `/download0` root and the conditional USB mount points `/usb0` through `/usb7`. A candidate volume is listed only if it is a directory, the app can create its `nests/roms` subdirectories, and a create/write/fsync/remove probe succeeds. The app displays the selected volume and its available space.

The preference is stored atomically in `/download0/nests/settings.txt` and records the selected volume ID plus either per-console or flat organization. Per-console files go under `roms/<console>/`; flat files go directly under `roms/`.

USB visibility, mount naming and write permissions depend on the console's loader/title context. The code probes the paths at runtime, but actual USB mounting, removal and reinsertion still require physical tests on the target PS5. If the selected USB device disappears, a download fails closed; it does not silently switch drives.

The browser preview cannot inspect PS5 mount points or write to `/download0`; it only demonstrates the settings presentation.

## Native HTTPS downloader

The native transport uses system `sceHttp`/`sceSsl` calls, a bounded 64 KiB read buffer and explicit cleanup of the network, SSL and HTTP contexts. It is intended for a sandboxed native title; do not mark the app elevated as part of this integration.

A download is accepted only when the supplied manifest record has:
- a direct HTTPS URL with the exact catalog-approved host (no custom port, userinfo or redirect);
- explicit redistribution permission, license/evidence checks and a verified catalog-signature flag;
- a positive declared size (maximum 2 GiB), safe filename/console path and a 64-character SHA-256.

The first version accepts HTTP 200 only and rejects all redirects. It creates a `.part` file on the selected volume, checks free space, checks cancellation between reads, writes all bytes, verifies the exact byte count and SHA-256, calls `fsync`, then renames the verified file into the library. Existing final files are not intentionally overwritten. Errors and cancellation remove the partial file.

Cancellation is checked between system HTTP reads; a read can remain blocked until its configured receive timeout (15 seconds) expires. Progress state and received/expected byte counts are exposed to the UI.

**Important integration boundary:** the native signed-catalog parser/Ed25519 verifier has not yet been connected to this downloader. The demo catalog records all have null download specs, so the UI currently refuses every demo download. Do not set the signature/rights flags from untrusted data or enable downloads until the signed catalog pipeline supplies those values.

## Controls

The expected SDL mapping is:
- D-pad: navigate;
- Cross / SDL A: favorite, save setting or start the selected download in Download Manager;
- Square / SDL X: request a download for the selected item;
- Triangle / SDL Y: open Download Manager; in the manager, request cancellation;
- Circle / SDL B: go back;
- Options / Start: open Settings.

Keyboard fallback for host testing: arrows to navigate, Enter/Space for the primary action, X to request a selected download, D to open Download Manager, C to cancel, Tab for Settings and Esc to go back. Confirm actual DualSense mapping on hardware.

## Host verification vs hardware verification

GitHub Actions compiles the source against host SDL2 headers and runs host tests with stubbed `sceHttp` responses. Those tests exercise path safety, internal/USB-like directory detection, settings persistence, SHA-256, successful mocked transfer, bad hash, redirect rejection, cancellation and removed-volume failure.

They do **not** validate PS5 SDK imports, real TLS, real USB mount behavior, throughput, or firmware 13.60. These remain hardware gates in `docs/TEST_PLAN_13_60.md`.
