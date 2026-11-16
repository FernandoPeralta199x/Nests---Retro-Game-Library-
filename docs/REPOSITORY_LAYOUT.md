# NESTS repository layout

The repository is the source of truth. Do not keep the only copy of code in a ZIP attachment.

- preview/: browser-based product preview; no PS5 APIs are implied by the browser build.
- catalogs/: versioned central manifests and JSON schemas.
- tools/: deterministic catalog and release tooling.
- tests/: host-side regression tests.
- native/ps5/: native shell source and build integration notes.
- docs/: architecture, security policy and hardware test evidence.

## Local preview

From the repository root:

    python -m http.server 8080

Open:

    http://localhost:8080/preview/

## Validate the catalog

    python tools/validate_catalog.py catalogs/stable.json
    python -m unittest discover -s tests -v

The demo catalog must remain non-downloadable. A verified record requires explicit redistribution permission, a license identifier, license URL, evidence URL, a publisher source page, an exact HTTPS download host in the manifest allowlist, a positive file size and a SHA-256 digest.

## Release rules

1. Never replace a demo record with an unauthorized commercial ROM.
2. Never interpret a listing on an external index as redistribution permission.
3. Keep personal backups out of shared manifests.
4. Require reproducible host checks before a release.
5. Record the exact package digest and physical-console evidence for PS5 claims.
