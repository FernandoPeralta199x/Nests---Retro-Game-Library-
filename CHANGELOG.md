# Changelog

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

- Added catalog v2 structure, validation and Ed25519 signing tools.
- Added read-only catalog serving and browser-side signature verification.
- Added a pure C++ download policy gate and SDL2 native UI shell proof of concept.
