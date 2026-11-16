# NESTS — hardware validation plan for PS5 firmware 13.60

**Status: NOT RUN.** A jailbreak firmware range is not proof that this native title, its SDL2 dependency, or the chosen loader combination works.

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

- [ ] Build from a clean native boilerplate checkout and record toolchain versions.
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

## Gate C — local library

- [ ] Add a demo favorite, close the app and relaunch.
- [ ] Confirm the favorite remains present after relaunch.
- [ ] Remove a favorite and confirm removal persists.
- [ ] Verify corrupt local data is rejected without crashing or exposing partial records.
- [ ] Verify no path outside /download0/nests/ is accessed.
- [ ] Confirm interrupted writes do not leave a truncated current file.

## Gate D — future network/download milestone (disabled in this build)

Do not mark this gate passed until native TLS, pinned Ed25519 verification and the streaming file writer are implemented and reviewed.

- [ ] Reject unsigned catalogs and unpinned publisher signatures.
- [ ] Reject HTTP, hostname mismatch, invalid certificates, untrusted redirects and non-default ports.
- [ ] Reject items without documented license and explicit redistribution permission.
- [ ] Enforce per-file and total storage limits before writing.
- [ ] Cancel downloads and clean temporary files.
- [ ] Verify exact size and SHA-256 before atomic promotion into the library.
- [ ] Test interrupted download, low storage, DNS failure, timeout and TLS failure.
- [ ] Confirm no partial or unverified file is marked installed.

## Exit criteria

The 13.60 native-shell milestone passes only after Gates A, B and C are completed on the physical console and evidence includes the exact artifact digest. Gate D is a separate release gate. Do not claim emulator compatibility until a specific emulator, BIOS policy, game format and launch path are independently tested.
