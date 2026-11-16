# NESTS — hardware validation plan for PS5 firmware 13.60

**Hardware status: NOT RUN.** A jailbreak firmware range is not proof that this native title, its SDL2 dependency, system HTTP imports, USB mount paths, or chosen loader combination works.

**Host-only work added in development 0.7:** C++ storage/download modules are compiled and exercised with mocked SCE HTTP responses in CI. This does not validate actual console TLS or USB behavior.

## Record before testing

- PS5 model:
- Firmware: 13.60 (record the full build string)
- Loader name/version:
- ShadowMountPlus version/configuration, if used:
- ELF loader version:
- NESTS .ffpfsc SHA-256:
- Source commit/archive SHA-256:
- SDL2 PacBrew module/version:
- Test date/operator:

## Gate A — package and launch

- [ ] Download the CI `.ffpfsc` artifact and record its SHA-256; this proves packaging, not hardware behavior.
- [ ] Confirm the PPSA title ID is unique on the console.
- [ ] Inspect the package for eboot.bin, sce_sys/param.json, SDL2 dependencies and NESTS assets.
- [ ] Refresh the homebrew loader list and confirm NESTS appears once.
- [ ] Launch NESTS and confirm it reaches the menu without a crash or black screen.
- [ ] Close from the PS5 shell and confirm the console remains responsive.

## Gate B — input and rendering

- [ ] Confirm output resolution and text readability at TV viewing distance.
- [ ] Test D-pad, Cross, Circle, Square, Options and controller disconnect/reconnect.
- [ ] Confirm held buttons do not trigger unintended repeated actions.
- [ ] Confirm DualSense works without keyboard fallback.
- [ ] Leave the app idle for 15 minutes and check frame pacing/memory stability.

## Gate C — local library and storage

- [ ] Confirm the default internal path resolves to /download0/nests/roms/.
- [ ] Connect a supported USB drive and confirm it appears only after the runtime write probe succeeds.
- [ ] Confirm the detected mount root is the correct physical USB device.
- [ ] Switch between internal and USB destinations and save the preference.
- [ ] Close/relaunch and confirm the selected volume/layout is restored.
- [ ] Remove the selected USB drive and confirm a new download fails closed without falling back to another device.
- [ ] Reconnect the drive and verify it is rediscovered after reopening Settings.
- [ ] Test low free space, read-only media, unsupported filesystem and filesystem removal/reconnect behavior.



- [ ] Add a demo favorite, close the app and relaunch.
- [ ] Confirm the favorite remains present after relaunch.
- [ ] Remove a favorite and confirm removal persists.
- [ ] Verify corrupt local data is rejected without crashing or exposing partial records.
- [ ] Verify no path outside /download0/nests/ is accessed.
- [ ] Confirm interrupted writes do not leave a truncated current file.

## Gate D — native HTTPS/download pipeline (host-tested, hardware NOT RUN)

The branch now contains a system sceHttp/sceSsl transport, a streaming SHA-256 writer, temporary-file promotion and a Download Manager view. The native signed-catalog parser/Ed25519 verifier is not yet connected, so current demo records remain non-downloadable. Do not enable shared downloads until the manifest signature and rights flags are supplied by the verified catalog pipeline.

- [ ] Connect the native catalog parser and pinned Ed25519 verification to the DownloadSpec factory.
- [ ] Reject unsigned catalogs and unpinned publisher signatures.
- [ ] Reject HTTP, hostname mismatch, invalid certificates, redirects and non-default ports.
- [ ] Reject items without documented license and explicit redistribution permission.
- [ ] Enforce per-file and volume free-space limits before writing.
- [ ] Cancel downloads and clean temporary files; record cancellation latency.
- [ ] Verify exact size and streaming SHA-256 before atomic promotion into the library.
- [ ] Test interrupted download, low storage, DNS failure, timeout and TLS failure.
- [ ] Confirm removal of USB during transfer removes .part and leaves no final file.
- [ ] Confirm no partial or unverified file is marked installed.

## Exit criteria

The 13.60 native-shell milestone passes only after Gates A, B and C are completed on the physical console and evidence includes the exact artifact digest. USB selection is a separate hardware sub-gate and must not be inferred from host directory tests. Gate D is a separate release gate. Do not claim emulator compatibility until a specific emulator, BIOS policy, game format and launch path are independently tested.
