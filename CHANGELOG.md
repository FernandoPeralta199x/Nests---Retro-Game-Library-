# Changelog

## 0.7 — USB storage and native download pipeline

- Added a native storage manager for the title-local internal data root and conditional USB mount points `/usb0`–`/usb7`.
- Added runtime directory/write probes, available-space reporting, persisted volume selection and flat/per-console path generation.
- Added a system `sceHttp`/`sceSsl` HTTPS download service with certificate verification flags, direct-host allowlisting, size limits, progress, cancellation checks and streaming SHA-256.
- Downloads are written to a temporary `.part` file and promoted only after size/hash verification; partial files are deleted on failure.
- Added a native Download Manager view and DualSense actions for settings, start and cancellation.
- Added host tests with mocked SCE HTTP responses for USB/internal path selection, settings persistence, safe paths, SHA-256, successful transfer, hash mismatch, redirects, cancellation and removed storage.
- Native signed-catalog parsing/Ed25519 verification is not yet wired; the six demo records remain non-downloadable.
- GitHub Actions now cross-compiles against a pinned PS5 build workspace and generates a `.ffpfsc` artifact; real SCE TLS, USB mount behavior and PS5 13.60 hardware validation remain separate NOT RUN gates.

## 0.6.1 — file storage preference

- Added a file-storage section to Settings with the PS5 destination root and organization mode.
- Added native persistent setting for a single ROM folder or per-console subfolders.
- Native shell creates the title-local /download0/nests/roms/ directory and stores the organization preference atomically.
- External USB storage remained unavailable pending mount and permission validation on firmware 13.60.

## 0.6 — repository source baseline + native shell proof of concept

- Added the browser preview as versioned source under preview/.
- Added a v2 catalog JSON Schema and fail-closed Python validator.
- Added automated regression tests for demo records, duplicate IDs, missing redistribution rights, insecure URLs and untrusted hosts.
- Added GitHub Actions to validate the central catalog and run host tests.
- Added an SDL2 native shell source with controller navigation and atomic favorites persistence in /download0/nests/.
- Added reproducible local development and native integration instructions.
- Firmware 13.60 hardware validation remains NOT RUN.

## 0.5 — native local library milestone

- Added a bounded, versioned native favorites store for /download0/nests/favorites.txt.
- Writes use temporary-file staging, fsync, atomic rename and best-effort parent-directory sync.
- Added strict game-ID validation, duplicate detection, bounded file reads and corrupt-file rejection.
- Connected favorites and the Library view to the SDL2 native PS5 shell.
- Added native C++ persistence and download-policy tests.
- Added central catalog startup/focus/periodic refresh with ETag conditional requests.
- Restricted the local development catalog server to loopback by default and narrowed CORS.
- Added a firmware 13.60 hardware test plan.

## 0.4 — signed catalog foundation

- Added catalog v2 structure, validation and Ed25519 signing workflow.
- Added a pure C++ download policy gate and SDL2 native UI shell proof of concept.
- The native shell is a host-tested prototype, not a firmware 13.60 compatibility claim.
