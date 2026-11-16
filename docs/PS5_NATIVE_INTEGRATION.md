# Native PS5 integration — storage and download milestone

## Current status

The NESTS native shell has an SDL2 store view, local favorites, a native storage-volume manager and a first HTTPS download service. GitHub Actions has a pinned cross-build and `.ffpfsc` packaging job; its artifact is still a homebrew filesystem image, not a Sony-signed retail PKG.

The system APIs below are based on the public PS5 native app boilerplate's networking recipe and update-check example. That recipe demonstrates `sceHttp`/`sceSsl` in a sandboxed native title and warns that the same route fails when the app is elevated. NESTS must remain sandboxed for this transport unless a separate verified network implementation is adopted.

## Storage model

- Internal destination: `/download0/nests/roms/`.
- Candidate USB roots: `/usb0` through `/usb7`.
- A candidate is listed only after runtime checks that it is a directory, app-readable, and able to create, write, sync and remove a probe file under `nests/roms`.
- The selected volume ID and layout mode are saved atomically to `/download0/nests/settings.txt`.
- The layout is either flat (`roms/<filename>`) or per console (`roms/<console>/<filename>`).

The scan only reports paths visible to the title. External volume visibility and write permissions can vary by firmware, loader and app context. USB selection and disconnection/reconnection must be tested on the physical PS5 13.60. The downloader rechecks the selected root and destination before and during the operation; it does not silently switch to another volume.

## Download transport

The first downloader uses system `sceNetPoolCreate`, `sceSslInit`, `sceHttpInit`, `sceHttpCreateTemplate`, `sceHttpCreateConnectionWithURL` and `sceHttpCreateRequestWithURL` calls. It enables the platform HTTPS verification flags (server certificate, hostname, validity period, known CA and SNI), disables automatic redirects, sets 15-second timeouts and accepts direct HTTPS HTTP 200 responses only.

The download worker:
1. validates HTTPS URL and exact host against the trusted catalog host;
2. requires explicit redistribution permission, license/evidence checks and a verified catalog-signature flag;
3. verifies path segments, expected size (up to 2 GiB), and SHA-256 syntax;
4. checks the selected storage is still mounted/writable and has enough free space;
5. writes to a unique `.part` file using a bounded 64 KiB buffer;
6. updates atomic progress and checks cancellation between HTTP reads;
7. verifies the exact received size and full SHA-256, then fsyncs and renames the file to its final name;
8. deletes partial output on any failure.

Redirects, insecure HTTP, untrusted hosts, missing rights evidence, bad hashes, short/oversized transfers, existing destination files and removed storage fail closed. Cancellation can take up to the active HTTP read timeout to be observed.

## What is not yet connected

The downloader API is implemented and has host tests, but the native catalog parser and Ed25519 verification are not yet wired to it. The current six demo entries have null download specs, so they are deliberately not downloadable. A later milestone must parse the signed catalog, validate the signature against a pinned public key, bind license/evidence metadata, and only then create trusted `DownloadSpec` objects.

Archive extraction, local ROM scanning, emulator discovery and game launching are also not implemented.

## Build environment

Use [drakmor/ps5-native-app-boilerplate](https://github.com/drakmor/ps5-native-app-boilerplate) at pinned commit `470695e0c557f99ff2df0e36e4df713c5e636526` as the build workspace. The workflow copies all NESTS native sources into its `src/`, renders the NESTS vector logo to the required 512×512 launcher PNG, configures the development title ID `PPSA91473`, and runs `make PACBREW_PACKAGES=sdl2 ffpfsc`. Confirm the title ID is unique before installation.

```bash
make PACBREW_PACKAGES=sdl2 app
make PACBREW_PACKAGES=sdl2 ffpfsc
```

The native network service uses system SCE HTTP/SSL imports rather than adding libcurl or OpenSSL to NESTS. The cross-build must still prove those imports are available in the chosen loader/firmware environment.

## Verification boundaries

CI performs host C++ compilation, mocked native-service tests, and a PS5 SDK cross-build/package step. A successful `.ffpfsc` build does not prove real SCE TLS behavior, USB mounts, installability or PS5 firmware 13.60 compatibility. Follow `docs/TEST_PLAN_13_60.md` and record artifact hashes and hardware evidence before marking any device gate passed.

## References

- https://github.com/drakmor/ps5-native-app-boilerplate
- https://github.com/leolobato/ps5-native-app-boilerplate/blob/main/docs/RECIPES.md
- https://github.com/leolobato/ps5-native-app-boilerplate/blob/main/examples/update-check/update_check.c
