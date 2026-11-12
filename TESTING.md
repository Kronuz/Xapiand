# Testing targets

The targets every phase gate (T13) measures Xapiand against. How to run each layer is in
[docs/dev/testing.md](docs/dev/testing.md); the engine's own rules are in strata's `docs/TESTING.md`, whose coverage table
this file carries.

## Coverage

Source-based coverage (`clang -fprofile-instr-generate -fcoverage-mapping`, `llvm-cov`) of `src/*.cc`, over the unit tests
(ctest), the restore corpora replayed and the docs E2E:

```sh
quiet light cov 60 "coverage" -- harness/coverage.sh OUTDIR     # builds build-cov, runs the workloads, prints the table
harness/coverage.sh report OUTDIR                               # the table again from OUTDIR's merged profile
```

`harness/coverage.sh` prints, for each area, lines and branches beside these targets (`STRICT=1` exits 3 when one is
below); `OUTDIR/coverage-files.txt` has every file. A line left uncovered is listed in the gate report with its reason.

| Code | Lines | Branches | Files (`AREAS` in `harness/coverage.sh`) |
|---|---|---|---|
| Decoders, parsers, format | >= 98% | >= 95% | `engine/strata/document.cc`, `engine/strata/pattern.cc`, `engine/codec.cc`, `field_parser.cc`, `serialise.cc`, `y2j/` |
| Scorers, collectors, counting | >= 95% | >= 90% | `engine/strata/search.cc`, `engine/strata/expand.cc`, `engine/search.cc` |
| Everything else | >= 90% | >= 80% | the rest of `src/`, one row per directory |

## Restore corpora

`tests/corpora/` holds one directory per writer, each a `--dump` of the index `rt` (`rt.mp`, with a `BUILD` line naming the
build that wrote it), and `source.ndjson`, the documents both builds indexed. `tests/corpora/README.md` names the builds.

```sh
harness/restore_corpora.sh check build/bin/xapiand              # RESTORE of each corpus over HTTP and with --restore, DUMP compared document by document
harness/restore_corpora.sh write build/bin/xapiand OUTDIR       # a new corpus from any build (BUILD_ID names its commit)
```

`harness/restore_corpora_gen.py OUT [N]` regenerates `source.ndjson` byte for byte.
