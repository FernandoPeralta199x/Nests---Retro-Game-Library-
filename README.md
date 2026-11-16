# NESTS — Retro Game Library

**Development 0.6 · repository source baseline + native shell proof of concept**

NESTS is a TV-first, multi-console retro homebrew catalog and personal library for a homebrew-enabled PS5. It is designed around search by game and console, artwork and metadata, favorites, local personal-file import, catalog refresh, and a secure download pipeline for content whose redistribution rights are explicitly documented.

## Product boundary

- Shared downloads are limited to homebrew, public-domain, open-source and freeware content where redistribution is expressly authorized.
- FMHY is a discovery index, not proof that a listed file may be redistributed.
- Personal backups imported by the user stay local and are never included in the shared catalog.
- This project does not host, scrape, or redistribute unauthorized commercial ROMs.

The central catalog currently contains six fictional demo records. No ROM file or commercial-game download URL is included, and all demo downloads are disabled.

## Repository status

The Git repository is the source of truth and includes the browser preview, catalog schema, validator, regression tests, CI workflow and native SDL2 shell source. Follow docs/REPOSITORY_LAYOUT.md for local development.

## Development 0.6

- Browser preview loads catalogs/stable.json rather than maintaining a second hard-coded catalog.
- Search and filters cover multiple retro systems.
- Favorites and personal imports are kept locally in the browser preview.
- Download controls remain disabled for demo and unverified records.
- Catalog validator enforces HTTPS, exact host allowlisting, explicit redistribution permission, license/evidence URLs, expected size and SHA-256 for verified downloads.
- GitHub Actions runs catalog validation and host regression tests.
- Initial native SDL2 shell source supports TV-sized rendering, controller navigation and persistent favorites in /download0/nests/.

## PS5 13.60

The native shell is not yet built, installed, or tested on a physical PS5 with firmware 13.60. Jailbreak availability does not prove native title, SDL2, loader or controller compatibility. Follow docs/TEST_PLAN_13_60.md and docs/PS5_NATIVE_INTEGRATION.md. Do not mark a hardware gate passed without an artifact digest and console evidence.

## Local development

Run the preview from the repository root:

    python -m http.server 8080

Then open http://localhost:8080/preview/.

Validate the central catalog and run regression tests:

    python tools/validate_catalog.py catalogs/stable.json
    python -m unittest discover -s tests -v

## References

- PS5 native app boilerplate: https://github.com/drakmor/ps5-native-app-boilerplate
- Pegasus DL+ (workflow reference only): https://github.com/pegasus-ps5/pegasus-dl

NESTS has its own interface, catalog model and native source. It does not copy Pegasus DL+ UI or code.
