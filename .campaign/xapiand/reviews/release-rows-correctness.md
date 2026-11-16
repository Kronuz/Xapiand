# release-rows review: correctness

Commit: prospect:7a3a54bbb8f0933d34bdf187f1d53dbd47f7004a
Verdict: CHANGES
Date: 2026-11-11

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The harness, the tests and the numbers hold up. Both blocking findings are in the report (A7),
and both are text fixes: A3's result and A5's reading of its own fuzz logs claim more than the
jobs show.

## Evidence I relied on

- 261111-201050-c7d247 (build at prospect:2cc69d4, exit 0). Its log asserts the five pins and
  "include/ equals 850d4e8's". `out/build-release/_deps` resolves to Xapiand's pins for every
  library Xapiand also declares: allocators, char_classify, static_string, task, unicode,
  snowball and analysis. `FETCHCONTENT_SOURCE_DIR_{STRATA,COMPRESSORS,HASHES,THREADPOOL}` point
  at the refs. `LastTest.log` holds the per-test numbers the report quotes.
- 261111-201053-f77c3a (parity), 261111-201056-33db85 (verify), 261111-201230-8ef3e1 (the pin's
  51 tests), 261111-201059-749f46 (stems, exit 1, the recorded A4 miss), 261111-201102-4af80d
  (fuzz build) and the 24 fuzz jobs. All ran at 2cc69d4 and exited 0, except the stems job.
- CI run 43153036741 at 2cc69d4, for prospect's own pins only.

## What I checked myself

- Composition (risk 1). Xapiand master `CMakeLists.txt:459-772` declares compressors, hashes and
  threadpool ahead of strata, and declares none of strata's other dependencies (fst, crc32c,
  termdict, varint, residency, fusefilter, completion, journal). So those three plus strata are
  the only overrides, as `rows.sh` asserts. Between `850d4e8` and `2cc69d4`, the only changes
  outside `include/` are tests, tools, bench, docs and the root `CMakeLists.txt` strata pin, which
  `FETCHCONTENT_SOURCE_DIR_STRATA` replaces. The `fuzz/*.cc` targets are unchanged.
  `rows.sh:46` really diffs `include/` against the pin's checkout.
- Parity (risk 2). `tools/parity.cc` compares count, docid and rank order at 1e-9 relative score
  error, for both counted and pruned. A docid swap is accepted only within a 1e-9 tie. The exit
  code fails on any count, top-10 (exact passes), pruned-vs-counted or live/unique miss. It reads
  only the `check_at_least == 10000` rows. `rank.jsonl` holds 419 of each of 0, 100 and 10000, so
  all 419 exact queries run.
- `operators_test`: the vector data `test/fixtures/xapian-ops/ops.jsonl` has 711 rows. That is
  669 compared, 4 ELITE_SET ties, 32 recorded-only (12 SYNONYM and 20 ELITE_SET) and 6 Xapian
  errors (4 SCALEN, 2 negative SCALE). Every row is accounted for, and the job's tally matches.
- The operator set Xapiand serves is `src/engine/query.h:42` `engine::Op` and
  `src/query_dsl.cc:230-260`.
- Aggregations (risk 3). `aggregate_test` `every_collector()` places all 16 `agg::kind` values
  (`include/prospect/aggregate.hh:387`). It compares `agg::render` byte for byte, which tests
  values, not counts. Xapiand master never includes `prospect/aggregate.hh`
  (`git grep` over `src/`); it serves `src/aggregations/`.
- Fuzzing (risk 4). I read all 24 `a1/out/fuzz-*.log` files for INITED versus DONE coverage and
  the last NEW line.
- Version fixtures. The versions block in `test/segment_test.cc` covers 11 current and 8
  format-12 fixtures. strata:5d742b2's `format.hh:78-79` sets `version` and `min_read_version` to
  `pre_freeze`, so all 10 probe versions are unknown.
- Numbers (risk 5), recomputed from the logs, and all equal the report's:
  - parity: 419/419, 157/419 and 0.0536 with deletes, and the per-operator tallies.
  - verify: 962 and 200 queries with 0 mismatches, 8 segments, 90,910 deletes and a 151.827 s
    merge.
  - every cell of the fuzz table.
  - stems: 45 names, 39 equal, and the word counts.
  - 118 refusals, 240 comparisons, 12,006 matches and 145,089 visitor matches.
  - The stem-name list in `rows.sh` matches the 45 names (47 minus none and unknown) in Xapiand
    `src/database/schema.cc:1629`.
- Probe 261111-223819-03f694 (`.scratch/review-release-rows-correctness/asan_rows.sh`) builds the
  eight row tests and `parity` with CI's ASan+UBSan flags (Debug, -O1) at Xapiand's pins, then
  runs them, plus parity at 4 segments and with deletes. Result: exit 0 in 912 s, all 8 tests
  passed, with no ASan or UBSan report in `ctest.log`. It reproduced the same numbers: 240
  comparisons, 118 refusals, 3,696 oracle rows and 145,089 visitor matches; parity 419/419 and,
  with deletes, 157/419, exact after the merge.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | A3 is reported as "pass" in the Results table, and the campaign counts it met, but the release's aggregations were never tested. Xapiand master serves `src/aggregations`, not prospect's collectors, so the row proves a component the release does not use. The quest's A3 asks for "every collector Xapiand serves", and the lead filed hunt row 91 as release-critical for the same reason. This is the same shape as A4, which the report records as a miss. | `docs/gates/release-rows.md` Results row A3 and "Scope"; Xapiand master has no `prospect/aggregate.hh` include under `src/`; `hunt.md:91` | Record A3 like A4: a miss at the release's level. prospect's collectors pass (job c7d247); the Xapiand-level row is hunt row 91, re-run when it lands. Do not count c7d247 as meeting A3. Alternatively, a recorded lead ruling that prospect's collectors suffice. |
| F2 | yes | gap | A5 says "coverage is flat across seeds, so the hour reached a plateau, not a frontier". The logs show the opposite. Every one of the 24 runs was still adding units in its last 0-8% of executions: segment seed 2's last NEW was at #2534 of 2534, fielddict seed 2's at #19587 of 19587. Coverage is flat because the runs barely left the seed corpus. Over the INITED replay coverage, postings gained 0 to 3 edges, values 8 to 18 and segment 60 to 102. fielddict's runs diverged, from 4,598 to 4,758, so it is not flat either. fuzz_segment's length limit stayed at `lim: 26` for the whole run. The "executions per run" counts include the ~989/844/279/756 replayed seeds. So about 10k (segment), 26k (postings), 68k (values) and 110k (fielddict) mutated inputs ran per decoder. Each run started from the same seed corpus and its new units were discarded (`rows.sh` `rm -rf "$work"`), so the hour is six independent 10-minute starts, not one hour of progress. The runs ran on wall-clock time at nice 19 next to the other row jobs, and CPU time was not measured, so "one CPU-hour" (the quest's A5) is not shown. | the 24 `~/.scratch/campaign/jobs/<fuzz job>/a1/out/fuzz-*.log` files (INITED vs DONE, last `NEW`); `tools/release_rows/rows.sh` `fuzz()` | Replace the plateau sentence with these numbers: mutated inputs per decoder, new coverage over the seed corpus, still growing at the end, independent restarts with discarded corpora, wall-clock at nice 19 with CPU time unmeasured. Then record whether A5 counts as met at this depth as the lead's ruling, as A4's is, citing hunt row 92. |
| F3 | no | gap | The A2 exclusions miss two served shapes. One is `Query::source`: `_range` value and geo ranges are posting sources in Xapiand's own `src/multivalue/range.cc` and `geospatialrange.cc`, the class the report already gives WILDCARD and EDIT_DISTANCE. The other is `match_all`, behind `_not`, `match_all` and `match_none`. Apart from these, the stated gaps (12 SYNONYM over non-term children, the two expansions) are the only uncompared rows. | Xapiand `src/query_dsl.cc:230-260,895-922`, `src/engine/query.h:42` | Add both to "Rows not compared" as Xapiand-level tests. |
| F4 | no | gap | A6 at the release pins: CI's ASan+UBSan run builds against prospect's own strata (`94480d5`). The report says so. The probe settles it: the row tests and parity pass under ASan+UBSan at Xapiand's pins. | report A6; probe 261111-223819-03f694 (exit 0) | Cite 261111-223819-03f694 in A6 as the sanitized run at the release pins. |
| F5 | no | nit | The parity "live and unique" check (`tools/parity.cc`, `h.ord < n`) cannot tell the deleted older versions in the `PARITY_DELETES` stream, which reuse docids 1 to N, from the live documents. Only the deleted extras past N are caught. The exact counts cover this, so the report's "every hit is a live document" is stronger than this check alone. | `tools/parity.cc` `stream.add(d + 1, ...)` and the `live` loop | Word it as "no deleted extra, no repeat", or check the stream segment's ordinals. |
| F6 | no | nit | In the deletes pass, the A2 table's "`parity` (each pass)" column reports e.g. 100/100 while only 157 of 419 top 10s matched. The `by_op` tally ignores the top 10 when the pass is not exact. | `tools/parity.cc` `equal = ... (!exact \|\| ok)`; job f77c3a log | Label the column "exact passes", or exclude the deletes pass. |
| F7 | no | nit | `parity`, `verify`, `stems` and `fuzz` do not re-assert the pins. They trust that `$HOME/.scratch/release-rows/<commit>` was filled by a `build` or `fuzz-build` that did. That holds today, since the directory is per commit and those steps always run `pins()`, but nothing stamps it. | `tools/release_rows/rows.sh` `bin=` and the steps without `pins` | Write the asserted pins into `$bin` at build time and check them in the consuming steps. |
| F8 | no | nit | `segment_test`'s version block checks the segment prefix version, which is strata's check. The doc-value column's own version byte (`include/prospect/docvalues.hh:281`) is refused as `corrupt`, not `unsupported`, and has no fixture. The segment version governing it makes that defensible; the report could say so. | `include/prospect/docvalues.hh:281` | One sentence in A5. |

## Not run

- I did not rerun any row job. The only job I ran is the ASan+UBSan probe above.
- I did not read Xapian's source or the oracle's code. I read only the oracle's vectors
  (`rank.jsonl`, `ops.jsonl`) as data.
- I did not judge whether A4's miss and A3's Xapiand-level gap may ship: that is the lead's and
  the owner's call.
- I did not check whether lz4 and zstd, which compressors fetches, match the Xapiand build's.
  Both are fetched through the same compressors pin.
- I did not check any platform other than macOS x86_64.
