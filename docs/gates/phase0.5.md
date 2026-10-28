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

## Lead verdict (2026-10-27)

Gate 0.5 is not passed yet. T2 and T4 pass. Three items stay open:

1. T1: the census (design D12, `ENGINE_CENSUS`) was never built. It is a small build step on the
   seam, then T1 and T5 are measured from it.
2. T3 throughput: bulk index runs at 0.81x base (3,124 against 3,860 docs/s), slower in all four
   interleaved pairs. It is bisected across the 19 first-parent commits from `855f75281` to
   `cca0011bb` before any fix.
3. T3 bodies: the `SEARCH /bank/` estimate reads 459 on base and 400 on ours with equal counts.
   It is attributed in the same bisect; an estimate change is acceptable only if it is explained.

The gate is rerun on the commit that closes all three.

## Run 2 (2026-10-27, master `7efc3753e`)

Master `7efc3753e98109a0cb0191fb892e73c2c730707b` (census, write path that builds documents once, A-T5, CR1 replay commit,
exact numeric ranges, exact cluster aggregations), detached worktree, Apple Silicon macOS, Release (`-DBUILD_TESTS=OFF`,
default LTO), plus an `ENGINE_CENSUS=ON` build for T1 and T5. Base: `855f75281`, the binary of run 1 (same commit). newman 6.2.3.
Throughput units ran one at a time under `quiet exclusive`; the other rows under `quiet light`.

| Row | Target | Result | Status |
|---|---|---|---|
| T1 census | `docs/dev/engine-census.md` from an `ENGINE_CENSUS` build | Build works; 339 requests in 339 lines (7 readiness probes, 11 unattributed lines, 0 tainted), 576 of 576 assertions. Per-class and facility tables regenerated (call counts moved; same facilities) | PASS |
| T2 seam closed | `harness/seam_check.sh` exits 0 | Exit 0: 7 files, 158 lines in the allowlist | PASS |
| T3 assertions | Zero assertion regressions | 339 requests, ours 576/576, base 574/576; 0 regressions, 0 pre-existing, 2 fixes (Basque) | PASS |
| T3 bodies | Differ only in volatile fields, or explained | 19 divergences by `bodydiff.py` (21 request-level differences in a path-level diff), all classified below: volatile ids and counters, Basque, and the exact aggregations; the bank estimate 459 became 232, the exact match count | PASS |
| T3 index throughput | Within the noise band | 0.996x base by trial mean (4456 vs 4476 docs/s; by run median 0.989x, 4470 vs 4520). Pairs 1.06, 0.95, 1.03, 0.94; every perfdiff verdict `~same`. Run 1 was 0.81x | PASS |
| T3 query QPS | Within the noise band | 0.974x base by trial mean (4178 vs 4291; by run median 0.984x). Pairs 1.07, 0.93, 0.94, 0.96, all inside the QPS band; p95 +10% in 2 of 4 pairs. Ours-ours A/A 4076 vs 4152, base-base 4375 vs 4410 | PASS (see note) |
| T4 cluster | `cluster_check.sh 2` and `3` incl. [3b] | Both GREEN; [3b] every node reports `_doc_count=40 _sum=530 _avg=13.25`, with and without `_check_at_least:1` | PASS |
| T4 WAL | `wal_recovery_check.sh`, `wal_replay_check.sh`, `wal_replay_agg_check.sh` | PASS 50/50; PASS (visible, walcommit, idempotent, failure); PASS (avg 19.5, max 39.0 after replay) | PASS |
| T5 key prefix | At most one `Q` term per document, the id term | Census over the suite: 2625 documents written, 2625 with exactly one `Q` term (473 named the key and it is that term, 2152 by docid), 0 with a second. `id_terms_check.py --census`: 97 documents, 0 with more than one `Q` term, 20 of 20 range queries correct | PASS |
| Values | `numeric_range_check.py` | 797 ranges over integer, positive, floating and datetime fields, 0 wrong | PASS |
| T11 | Recorded only | Not measured: `harness/profile_bulk.sh` is not on master | n/a |

### Run 2 body divergences (`bodydiff.py` and a path-level diff of the raw reports)

1. `POST /twitter/user/`, `POST /test/comment-ignore/`, `RESTORE /test/restore/`: generated ids. Volatile.
2. `GET /` (`server/revision`, `versions/Xapiand`) and `GET /:metrics` counters. Volatile.
3. `PUT /test/stemlang/basque/doc` 500 to 200 and its search 0 to 1 hit: intentional (basque mapped to `eu`, `58bdfdd83`).
4. 14 `SEARCH /bank/` requests differ in `aggregations._doc_count` (for example 38 to 91, 6 to 64, 56 to 991) and in `total`
   (for example 400 to 349, 700 to 461, 1000 to 443). Intentional, `9dd443095` and `47d0db117`: aggregations now check every
   match (`engine::CHECK_ALL`). Base counted only the documents its match spy was shown and reports `total` as a match estimate; ours checks all
   matches, so `_doc_count` and `total` are the true count where they changed.
5. The bank estimate, `_and [gender female, age 20..30]`: base `total` 459 (run 1 ours 400), now 232 with `_doc_count` 232.
   A brute-force count over `docs/public/assets/accounts.ndjson` gives 232 (953 rows), so 232 is the true match count and the
   old 459 and 400 were estimates. Unchanged requests (`_doc_count` 349, 211, 461, 232 on both) confirm the base is exact
   when the query's matches are all checked.

### Notes

1. One base unit stalled (a RESTORE took 64 s and the connection died on trial 2). The unit was rerun as `base1r`; the
   rerun is the base1 used above. Not seen on ours in 7 units.
2. The query mix includes aggregations, which now visit every match; the 2.6% lean on QPS fits that cost and sits inside the
   band, so the row passes, but it is the number to watch.
3. Disk bytes after a bulk load are equal within 1.5% (about 12.8 to 13.1 MB).
4. Results, binaries, scripts: `.scratch/gate05b/` in the main Xapiand checkout. The census tables in
   `docs/dev/engine-census.md` ("Per request class", "Facility totals") are regenerated from this run.
