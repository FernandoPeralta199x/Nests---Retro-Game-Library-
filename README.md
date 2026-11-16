# NESTS — Retro Game Library

**Development 0.7 · USB storage selection + native HTTPS download pipeline**

NESTS is a TV-first, multi-console retro homebrew catalog and personal library for a homebrew-enabled PS5. It is designed around search by game and console, artwork and metadata, favorites, local personal-file import, catalog refresh, storage selection and a secure download pipeline for content whose redistribution rights are explicitly documented.

## Product boundary

- Shared downloads are limited to homebrew, public-domain, open-source and freeware content where redistribution is expressly authorized.
- FMHY is a discovery index, not proof that a listed file may be redistributed.
- Personal backups imported by the user stay local and are never included in the shared catalog.
- This project does not host, scrape, or redistribute unauthorized commercial ROMs.

The central catalog still contains six fictional demo records. No ROM file or commercial-game download URL is included, and all demo downloads remain disabled.

## Development 0.7

- Browser preview continues to load `catalogs/stable.json` and demonstrates library/configuration behavior.
- Native settings scan internal storage plus conditional USB mount points `/usb0` through `/usb7`; volumes are shown only when a runtime write probe succeeds.
- The selected volume and flat/per-console layout are persisted atomically in `/download0/nests/settings.txt`.
- Native Download Manager exposes progress/status/cancel controls through SDL2/DualSense.
- Native downloader uses system `sceHttp`/`sceSsl` over direct HTTPS only, with platform certificate checks, exact host policy, size validation, streaming SHA-256 and temporary-file promotion.
- Host tests cover storage selection, persistence, path traversal rejection, mocked successful transfers, redirects, bad hashes, cancellation and removed USB volumes.
- Native catalog signature verification is not yet connected to the downloader; all current demo game entries have no download package and are blocked by design.

## PS5 13.60

The code has not yet been cross-compiled, installed or tested on a physical PS5 13.60. USB roots are conditional and must be checked in the real loader/title context. Host tests use mocked SCE HTTP services; they do not prove real TLS, USB mount behavior, loader imports or hardware compatibility.

Follow `docs/PS5_NATIVE_INTEGRATION.md` and `docs/TEST_PLAN_13_60.md`. Do not mark a hardware gate passed without an artifact digest and console evidence.

## Local development

Run the browser preview from the repository root:

```bash
python -m http.server 8080
```

Open `http://localhost:8080/preview/`.

Validate the catalog and Python tests:

```bash
python tools/validate_catalog.py catalogs/stable.json
python -m unittest discover -s tests -v
```

Host-native storage/download tests are compiled and executed in GitHub Actions by `.github/workflows/validate.yml`.

## Build target

GitHub Actions cross-compiles the native shell against the pinned public [PS5 native app boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate), links SDL2 from PacBrew, and creates a compressed `.ffpfsc` artifact for each successful build. The workflow uses the development title ID `PPSA91473`; confirm it is unique on the console before installation. The artifact is a homebrew filesystem image, not a Sony-signed retail PKG.

```bash
make PACBREW_PACKAGES=sdl2 app
make PACBREW_PACKAGES=sdl2 ffpfsc
```

The SCE HTTP/SSL imports and SDL2 runtime still require target cross-build and hardware validation.

## References

- https://github.com/drakmor/ps5-native-app-boilerplate
- https://github.com/leolobato/ps5-native-app-boilerplate/blob/main/docs/RECIPES.md
- https://github.com/leolobato/ps5-native-app-boilerplate/blob/main/examples/update-check/update_check.c
- https://github.com/pegasus-ps5/pegasus-dl (workflow reference only)

NESTS has its own interface, catalog model and native source. It does not copy Pegasus DL+ UI or code.
