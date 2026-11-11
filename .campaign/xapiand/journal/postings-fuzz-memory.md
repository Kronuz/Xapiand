# postings-fuzz-memory: journal

State: blocked on stored-check-cache: lands first; then the lead moves prospect's strata pin and A3 and A4 rerun
Quest: .campaign/xapiand/quests/postings-fuzz-memory.md (the lead's; never edit it)

## Doing

Waiting on A1's job (261110-180348-714558) and the build-test check (261110-180354-e4b1ac) at
prospect:10870d9. The lead's ruling (option 2): the strata fix is its own quest, stored-check-cache
(strata:71750940, fix and `test/reader_check_test.cc`), which lands first.

## Next

1. After stored-check-cache lands, the lead moves prospect's strata pin to it (CMakeLists.txt).
2. At that head: A3, one CPU-hour of fuzz_postings (ASan+UBSan) with `fuzz/rss.sh` logging RSS; A4, the build-test check.

## Blocked on

stored-check-cache landing, then the lead's pin bump. Lead ruled option 2 (msg from coordinator, 2026-11-10).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261110-180348-714558 (queued) at prospect:10870d9, strata 9d4630c: 10 min ASan+UBSan with RSS every 30 s, as is and with the stored check's frame cache. Local probes below |
| A2 | Ruled: the postings decoder allocates from no untrusted length (audit under Rulings), so it has no bound to add; what grew is strata's stored check, whose test fails without its fix in stored-check-cache (`test/reader_check_test.cc`, strata:71750940; revert job 261110-182037-74c93c) |
| A3 | Runs once prospect's strata pin moves to the landed stored-check-cache fix |
| A4 | build-test 261110-180354-e4b1ac (queued) at prospect:10870d9; reruns once the strata pin moves to the landed fix |

Local probes (quiet light, macOS x86_64, Homebrew clang 23, seed 1, corpus copy, 5 min unless
noted; logs in the code worktree's `.scratch/p*/`):

| Run | RSS over time (MiB) | Executions |
|---|---|---|
| ASan+UBSan as built, 10 min | 1,280 at 60 s, 1,881 at 240 s, peak 2,095 (default `-rss_limit_mb=2048` kills it near 9 min) | 4,914 (8/s) |
| Live heap at `-rss_limit_mb=768` | OOM after 32 inputs: 27.7 MB live (21 MB is libFuzzer's own), 243 MB quarantined | |
| ASan, `quarantine_size_mb=0` | 43 to 51, flat | 2,462 |
| ASan, `allocator_release_to_os_interval_ms=1000` | 1,112 at 30 s, 2,053 at 300 s | 2,547 |
| `-fsanitize=fuzzer` only | 78 to 104, flat after warm-up | 7,995 |
| ASan, strata check with frame cache | 325 at 30 s, 380 from 120 s on, peak 400 | 9,393 (31/s) |

Per-input allocation by the target (counting `operator new`, no sanitizer, 841 corpus inputs):
mean 19.9 MB, max 189 MB, while peak live is at most 4.5 MB; with the frame cache mean 4.1 MB,
max 7.5 MB. Corpus replay 13.1 s to 3.0 s with the cache (4.4x faster).

## Result

Pending the fix.

## Rulings I made

- A2: the conditional does not apply to prospect's postings decoder (audit below); the test that
  fails without the fix lives in strata with the fix (stored-check-cache).
- Attribution: neither the postings decoder nor libFuzzer's corpus (170 KB) grows. The target's
  `check_segment` call churns up to 189 MB per input through strata's stored check; ASan's
  256 MB quarantine of that churn keeps its allocator's regions growing (RSS flat with quarantine 0,
  without ASan, and with the cache). Large-block churn alone stays flat under ASan
  (`.scratch/churn/churn.cc`: 280 MB over 20,000 rounds), so the 28 KB churn volume is the cause.
- Postings decoder audit (security lens): every allocation is bounded by bytes or by the document
  count. `postings_iterator` checks the group count against range / entry size and the pairs,
  records and blocks regions against the term's range (`reader.hh:723, 728`); `positions_reader`
  bounds a read's count by `max_count_` and the stream's bytes before growing `out_`
  (`reader.hh:1531-1533`), `count_tail_` holds at most 127 entries, `tail_values_` reserves at most
  128; `order_` and `group_bounds_` (`search.hh:401, 482`) take the byte-bounded group count; the
  check's `postings` vector holds decoded postings, strictly increasing below the document count.
- `fuzz/rss.sh` (prospect:10870d9) samples a fuzz run's RSS; the A1 and A3 runs log with it.
