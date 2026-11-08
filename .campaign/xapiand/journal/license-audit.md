# license-audit: journal

State: landed
Quest: .campaign/xapiand/quests/license-audit.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:5caed332b on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/license-audit.md`: "`NOTICE:1-26` (owner 2026, Dubalu 2015-2020, MIT pointer, Elasticsearch section)" |
| A2 | `docs/status/license-audit.md`: "`docs/src/content/docs/copyright.md:6-31` matches `NOTICE`." |
| A3 | `docs/status/license-audit.md`: "`AUTHORS:1` is `german.mb@gmail.com`." |
| A4 | `docs/status/license-audit.md`: "`src/engine/search.cc:2`, `src/exception_engine.h:2`, `tests/fuzz_body.cc:2` carry the owner's line." |
| A5 | `docs/status/license-audit.md`: "17 `src/` files (`src/tls.cc:2`, `src/debounce_or_run.h:2`, `src/xapiand_*_trace.h:2`, ...)" |
| A6 | `docs/status/license-audit.md`: "Headers on 57 `harness/` scripts (`harness/env.sh:1`), the Dockerfile" |
| A7 | `docs/status/license-audit.md`: "`config.h.in:2-3`, `package_config.h.in`, `cmake/cmake_uninstall.cmake.in`" |
| A8 | `docs/status/license-audit.md`: "and the generated `THIRD-PARTY-NOTICES` name `contrib/python/xapiand-py`, its Apache-2.0 `LICENSE`" |
| A9 | `docs/status/license-audit.md`: "`contrib/javascript/cuuid/src/bx.js:11-34` has bs58's MIT text with Google Inc 2011 and BitPay 2013." |
| A10 | `docs/status/license-audit.md`: "`contrib/geospatial/google_map_plotter.py:14` has the MIT text" |
| A11 | `docs/status/license-audit.md`: "The 2019 import commit `Xapiand:7fe1e367f` names `github.com/tangrams/y2j` at 45396f91." |
| A12 | `docs/status/license-audit.md`: "`cmake/Modules/GetGitVersion.cmake:1-5` name google/benchmark and Apache-2.0" |
| A13 | `docs/status/license-audit.md`: "`cmake/Modules/CheckPrototypeExists.cmake:1-23` has KDE's BSD-3 text" |
| A14 | `docs/status/license-audit.md`: "`cmake/Modules/CheckDIRSymbolExists.cmake` is gone" |
| A15 | `docs/status/license-audit.md`: "`docs/public/assets/htm.png` (1800x1152) is a raster of `htm.svg` (the owner's Illustrator export)" |
| A16 | `docs/status/license-audit.md`: "`NOTICE:24-26` and `copyright.md:30-31` keep the Elasticsearch documentation attribution." |
| A17 | `docs/status/license-audit.md`: "`cuuid:1df1c06`, `hashes:5791db9`, `scheduler:f080858`, pushed to each `main`" |
| A18 | `docs/status/license-audit.md`: "`docs/package.json:6` and the root entry of `docs/package-lock.json` say" |
| A19 | `docs/status/license-audit.md`: "`CMakeLists.txt:1309-1310` installs `LICENSE`, `NOTICE` and `THIRD-PARTY-NOTICES` into `share/doc/xapiand`." |

## Result

Landed at Xapiand:5caed332b on 2026-11-03. No review named in the ledger.

- Merge: Record the build and test result in the license ledger.
- `Xapiand:6a9218ef2` branch build exits 0; ctest -j4 passes 37 of 37.
- `cmake --install` stages `share/doc/xapiand/{LICENSE,NOTICE,THIRD-PARTY-NOTICES}`.
- Actions A1, A2, A4-A8, A10-A16, A20, A22-A25 are recorded as done.
- Actions A3, A9, A17, A18, A19, A21, A26 and A27 remain undone by instruction.
- The three blockers remain: GPL sortable code and Python port, and unlicensed Lenna images.
- Docs: `docs/mechanisms/third-party-notices.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
