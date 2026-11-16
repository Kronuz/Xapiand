> Status: CURRENT (2026-11-03) the notice files and their install rule as built by the Phase 8 license actions; the audit is `docs/status/license-audit.md`.

# Third-party notices

## What

Every package carries `LICENSE` (MIT), `NOTICE` (who owns what, and the Apache-2.0 client and docs attribution) and `THIRD-PARTY-NOTICES` (the license text of every bundled component) in
`share/doc/xapiand`.

## How

`install(FILES LICENSE NOTICE THIRD-PARTY-NOTICES ...)` in `CMakeLists.txt` puts the three files in `${CMAKE_INSTALL_DATAROOTDIR}/doc/xapiand`. The deb, rpm, FreeBSD pkg, Docker image and
Homebrew bottle all build from the install tree, so they take the files with it.

`cmake/generate_third_party_notices.py DEPS_DIR OUTPUT` writes `THIRD-PARTY-NOTICES`. `DEPS_DIR` is a configured build's `_deps` directory, so the text is the license of the pinned
sources. Three modes:

1. `file` copies the dependency's own license file (rapidjson, cppcodec, CLI11, lz4's `lib/`, prometheus-cpp, libyaml, Snowball, the Unicode tables, http-parser, uint128_t, prospect's Lucene `NOTICE`).
2. `lua` copies the license block out of `lua.h` (the Lua tarball has no license file).
3. `name` lists only the copyright lines of the BSL-1.0 components (asio, static-string, msgpack), which need no binary notice.

The Kronuz libraries that embed third-party code (allocators, compressors, enum-reflection, boolean-parser, cuuid, hashes, scheduler) carry the notice in their `LICENSE`;
the script still reads the leading comment of the embedding header, which holds the same text, so it works at a pin before and after that change. md5 and sha256 (zlib) are read from
their headers only. `src/y2j/LICENSE`, the Olly Betts
permission for `src/length.{cc,h}` and the temporary note for `src/sortable_serialise.{cc,h}` are written in by the script.

Rerun the script at every repin and at the release commit; a run against the master pins reproduces the committed file byte for byte.

## Limits

The stopword lists and `mime.types` (installed in every package) have no recorded source, so `THIRD-PARTY-NOTICES` does not cover them (audit A17, A18).
