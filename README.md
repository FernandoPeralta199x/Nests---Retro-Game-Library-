# NESTS — Retro Game Library

NESTS is a multi-console retro homebrew and personal-library project for PS5. The product direction is a television-first catalog with console/game search, metadata and covers, favorites, a local library, and a safe download pipeline for items whose redistribution rights are explicitly documented.

## Repository status

The initial source package is being prepared as a versioned archive while the repository is initialized. The archive contains the preview application, catalog schema and validator, Ed25519 signing tools, local development API, native C++ download policy, SDL2-based native UI proof of concept, tests, and build notes.

## Legal and safety boundary

- Catalog releases are limited to homebrew, public-domain, open-source, and freeware content for which redistribution is expressly permitted.
- FMHY is a discovery index, not evidence that a listed file may be redistributed.
- Personal backups are imported by the user and remain local.
- The project does not host, scrape, or redistribute unauthorized commercial ROMs.

## PS5 target

Target firmware: **13.60**.

Jailbreak availability does not by itself prove that a native application or its dependencies are compatible. The NESTS native package has not yet been built or validated on a physical PS5 running 13.60.

## Technical direction

- Web preview for rapid UI iteration.
- Versioned JSON catalog with explicit rights and source evidence.
- Ed25519 signatures with publisher public keys pinned in the app.
- HTTPS-only downloads, exact host allowlists, size limits, SHA-256 verification, cancellation and temporary-file promotion in the production native implementation.
- C++/SDL2 native interface and DualSense/gamepad navigation.
- Local-only library storage.

## Reference projects

- [PS5 native app boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate) — build environment reference.
- [Pegasus DL+](https://github.com/pegasus-ps5/pegasus-dl) — workflow reference only; NESTS does not copy its UI or source.

See the source archive and docs in this repository for current implementation details.