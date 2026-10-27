# Gate 0.5 (X5): Xapiand on the engine seam

Run on Xapiand master `cca0011bb` (X1 to X4c merged), 2026-10-27, Apple Silicon macOS, Release builds
(`-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF`, default LTO). Base: `855f75281`, the parent of the first X1
commit (`fe9307227`), i.e. the Phase 0.5 branch point before X1. Rows are from `xapiand-on-strata.md` section 5.

## Results

| Row | Target | Result | Status |
|---|---|---|---|
| T1 census | `docs/dev/engine-census.md` from an `ENGINE_CENSUS` build | The `ENGINE_CENSUS` option does not exist on master (no match in `CMakeLists.txt` or the tree). Stopped; nothing built | NOT RUN (option missing) |
| T2 seam closed | `harness/seam_check.sh` exits 0 | Exit 0: 7 files, 158 lines in the allowlist | PASS |
| T3 assertions | Zero assertion regressions | 339 requests, ours 576/576, base 574/576; 0 regressions, 2 fixes (Basque) | PASS |
| T3 bodies | Differ only in volatile fields | 6 divergences: 4 volatile, 1 intentional, 1 unexplained non-volatile (`total` estimate 459 vs 400 on one bank search) | FAIL (one unexplained diff) |
| T3 throughput | Within the noise band | Query QPS 0.99x base (4133 vs 4163, within band). Bulk index docs/s 0.81x base (3124 vs 3860): slower in all 4 interleaved pairs, ranges do not overlap | FAIL (index regression) |
| T4 cluster and WAL | `cluster_check.sh 2`, `3`, `wal_recovery_check.sh` pass unchanged | All three exit 0 (GREEN, GREEN, PASS 50/50) | PASS |

## T3 diff classification

`e2e_diff.py ours base`: 0 regressions, 0 pre-existing, 2 fixes, 1 status difference.

1. `PUT /test/stemlang/basque/doc` 500 to 200 and `SEARCH /test/stemlang/basque/` 0 to 1 hit: intentional,
   `58bdfdd83` (basque mapped to code `eu`; base answers "Language code ue unknown"). Not a regression.
2. `POST /twitter/user/` auto-id: volatile.
3. `GET /`: `server/revision` and `versions/Xapiand` build stamps: volatile.
4. `GET /:metrics` counters: volatile.
5. `SEARCH /bank/` (bodydiff row): `took` is volatile everywhere, but the "Example" search for
   `_and [gender female, age 20..30]` (execution 168) has `total` 459 on base, 400 on ours, with equal `count`
   10 and `_doc_count` 232. `total` is a match estimate. Deterministic-looking, non-volatile, cause not
   identified (candidates: X1 term generation, X3 query build, X4 search). Not bisected, single capture each.
   Classified: regression-candidate, needs attribution.
6. The two intentional changes the lead listed (syntax errors 400, stale snapshot retry) do not appear:
   no request in the 339 exercises them.

## T3 throughput detail

`loadtest.py` defaults (accounts x20 = 20000 docs RESTORE, 16 clients, 8 s, 3 trials), one fresh node per run,
`quiet exclusive` hold, interleaved base1 ours1 base2 ours2 base3 ours3 base4 ours4.

| Metric | base runs | ours runs | ratio |
|---|---|---|---|
| index docs/s | 4139 3548 4056 3697 (mean 3860) | 3253 3031 3238 2972 (mean 3124) | ours 0.81x base |
| query QPS | 4222 4063 4184 4183 (mean 4163) | 4017 4220 4141 4154 (mean 4133) | ours 0.99x base |
| on-disk bytes | about 12.85 MB | about 12.85 MB | equal |

A/A (base vs base, ours vs ours, perfdiff): index spread base1 to base4 up to 14% (4139 vs 3548), ours-ours up
to 7%; QPS A/A within 5%. perfdiff base-to-ours per pair: index REGRESSED in 3 of 4 pairs (-21%, -15%, -20%),
~same flagged at -20% in the fourth only because its band was 21%; query p95/p99 mixed (+11% to -6%), no
consistent direction. Verdict: bulk indexing is about 19% slower than base; query throughput at parity. The
cause is not investigated (the X1 analysis swap and the X2 write-path seam are both candidates).

## Commands

```sh
# worktrees and builds (quiet light, -j4)
git worktree add --detach ~/code/Kronuz/Xapiand-wt/gate05 master
git worktree add --detach ~/code/Kronuz/Xapiand-wt/gate05-base 855f75281
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF && cmake --build build -j4 --target xapiand
# T2
harness/seam_check.sh
# T3 (newman 6 on PATH; system newman 4.5 yields no responses); same collection for both binaries
export PORT=18951
harness/e2e_capture.sh $BASE_BIN res/base.json DATADIR; harness/e2e_capture.sh $OURS_BIN res/ours.json DATADIR
python3 harness/e2e_assert.py res/ours.json
python3 harness/e2e_diff.py res/ours.json res/base.json; python3 harness/bodydiff.py res/base.json res/ours.json
# per run: fresh node on :18951, then loadtest (quiet exclusive, interleaved base/ours x4)
python3 harness/loadtest.py --target localhost:18951 --datadir DATA --label L --out res/load-L.json
python3 harness/perfdiff.py res/load-base1.json res/load-ours1.json
# T4 (quiet strict)
harness/cluster_check.sh 2; harness/cluster_check.sh 3; harness/wal_recovery_check.sh
```

`wal_recovery_check.sh` writes under `/tmp` by its own design; run unchanged.
