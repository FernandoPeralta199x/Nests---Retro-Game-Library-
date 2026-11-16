## 0.6.1 — file storage preference

- Added a file-storage section to Settings with the PS5 destination root and organization mode.
- Added native persistent setting for a single ROM folder or per-console subfolders.
- Native shell creates the title-local /download0/nests/roms/ directory and stores the organization preference atomically.
- External USB storage remains unavailable pending mount and permission validation on firmware 13.60.
- The native downloader is still not implemented; this setting prepares its destination and does not claim that ROM downloads are active.

# Changelog

## 0.6 — repository source baseline + native shell proof of concept

- Added the browser preview as versioned source under preview/.
- Added a v2 catalog JSON Schema and fail-closed Python validator.
- Added automated regression tests for demo records, duplicate IDs, missing redistribution rights, insecure URLs and untrusted hosts.
- Added GitHub Actions to validate the central catalog and run host tests.
- Added an SDL2 native shell source with controller navigation and atomic favorites persistence in /download0/nests/.
- Added reproducible local development and native integration instructions.
- Network transfer, native TLS, Ed25519 verification, archive extraction and emulator launching remain disabled/not implemented.
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
- Native downloads remain disabled until TLS, pinned Ed25519 verification, streaming SHA-256 and atomic promotion are implemented and validated.

## 0.4 — signed catalog foundation

- Added catalog v2 structure, validation and Ed25519 signing workflow.
- Added a pure C++ download policy gate and SDL2 native UI shell proof of concept.
- The native shell is a host-tested prototype, not a firmware 13.60 compatibility claim.
