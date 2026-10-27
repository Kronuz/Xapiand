# census (phase 0.5, gate rows T1 and T5)

Branch `phase0.5/census` from Xapiand master `8170a3ef6`. Design: strata `docs/design/xapiand-on-strata.md` D12, section 2,
gate rows T1 and T5. Local only, not pushed. Report: `docs/dev/engine-census.md`; mechanism: `docs/mechanisms/engine-census.md`.

## State

Built and proven locally; ready for review. Not pushed, no CI run.

## Done

1. `-DENGINE_CENSUS=ON` (off by default): counters at the seam, one JSON line per HTTP request
   (`XAPIAND_ENGINE_CENSUS=file`). Hooks are one line each in `src/engine/xapian/{search,shard,convert,callbacks}.cc` and
   `SearchService::handle`; no edit to `document.h`, `convert` bodies, `src/database/*`. Xapiand:cdfdba558 and the follow-up
   commit (T5 candidate prefixes, report, docs).
2. T5 check in `census::document`, counted not asserted, for both candidate prefixes `Q` and `_id.`.
3. E2E run with the census build: 339 requests, `e2e_assert.py` GREEN; results in `docs/dev/engine-census.md`.
4. Proof.
   - Tests: `tests/test_engine_census.cc`, 67 checks; with `engine_search` 4417, `engine_shard` 52, `engine_read` 657,
     `engine_query` 22395 all pass, Release census build and ASan (RelWithDebInfo, Apple clang) build.
   - Mutants, each caught: (1) `docview` ignores the calling callback (5 checks fail), (2) the T5 "other term" guard removed
     (1 fails), (3) the `ENGINE_CENSUS_SEARCH` hook removed from `engine::search` (3 fail).
   - Default build unchanged: `nm` finds 0 census symbols and `strings` 0 `XAPIAND_ENGINE_CENSUS`; `size` segments are
     identical to the gate 0.5 master binary (`__TEXT` 6635520, `__DATA` 2220032); file 8622032 vs 8621992 bytes (build path
     strings). The census build is 8657024 bytes (+34992).
   - `harness/seam_check.sh` exit 0, 7 files, 158 lines (allowlist unchanged).

## Next

1. Lead: read the "Proposed update to the design, section 2" in `docs/dev/engine-census.md`; two findings change the design:
   the key prefix is `Q`, not `_id.`, and a numeric id's accuracy terms break T5; all 94 searches ran over 5 shards.
2. When the seam write path branch lands, the strata engine adds the same hook lines (`prepare`, `search`, shard ops,
   `DocView` methods, `census::document`).
3. Not done: a second capture for variance (counts of calls depend on data; request counts are exact), a multi-node
   census, CI.

## Blocked on

Nothing.
