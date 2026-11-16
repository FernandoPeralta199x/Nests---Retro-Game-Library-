# NESTS — Retro Game Library

**Development 0.5 · TV-first catalog + native PS5 proof of concept**

NESTS is a multi-console retro homebrew catalog and personal library for PS5. The target experience is a TV-oriented storefront with search by console and game, metadata and artwork, favorites, a local library, centralized catalog refresh, and a secure download pipeline.

## Current repository contents

This repository currently tracks the project overview, changelog, the central demo catalog, and the firmware 13.60 hardware test plan. The complete v0.5 development source snapshot is distributed separately with this handoff; it contains the web preview, catalog schema and validator, Ed25519 signing tools, local catalog API, native C++ modules, SDL2 shell source, and host tests.

- Central catalog: [catalogs/stable.json](catalogs/stable.json)
- Changelog: [CHANGELOG.md](CHANGELOG.md)
- Firmware test plan: [docs/TEST_PLAN_13_60.md](docs/TEST_PLAN_13_60.md)

## Product and legal boundary

- Shared downloads are limited to homebrew, public-domain, open-source, and freeware content where redistribution is expressly authorized.
- FMHY is a discovery index, not proof that a listed file may be redistributed.
- Personal backups imported by the user stay local.
- This project does not host, scrape, or redistribute unauthorized commercial ROMs.

The current central catalog contains six fictional demo records. It contains no ROM files, commercial-game download URLs, or enabled downloads.

## Development 0.5 status

- TV-oriented web store with multi-console search, filters, details, favorites and personal-file import/export.
- Central catalog refresh on startup, app focus and a periodic interval; ETag conditional requests where supported.
- Catalog v2 validator and Ed25519 signing workflow.
- Download policy requiring a pinned catalog signature, explicit redistribution rights, HTTPS host allowlist, expected size and SHA-256.
- Native C++ download-policy core with host tests.
- Native SDL2 menu shell with gamepad input and favorites persistence under the PS5 title's `/download0/nests/` storage.
- Native download transport, native Ed25519 integration, archive extraction and emulator launching are not implemented yet.

## PS5 target: firmware 13.60

Jailbreak availability is not the same as native-app compatibility. The NESTS native package has not been built, installed, or tested on a physical PS5 running firmware 13.60. Follow the explicit test gate in [docs/TEST_PLAN_13_60.md](docs/TEST_PLAN_13_60.md). Do not treat a host build or syntax check as a console test.

## Technical references

- [PS5 native app boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate) — build environment reference.
- [Pegasus DL+](https://github.com/pegasus-ps5/pegasus-dl) — workflow reference only; NESTS does not copy its UI or source.

The NESTS interface, catalog model and native modules are original project work.