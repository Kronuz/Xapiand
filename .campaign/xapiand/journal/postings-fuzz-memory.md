# postings-fuzz-memory: journal

State: waiting on jobs 261110-192311-518a9b 261110-192324-fe3397
Quest: .campaign/xapiand/quests/postings-fuzz-memory.md (the lead's; never edit it)

## Doing

stored-check-cache landed (strata:71750940 on main); prospect's strata pin moved to it at
prospect:35a9886. A3 (261110-192311-518a9b) and A4's build-test (261110-192324-fe3397) queued at
that head. The branch is not pushed: contributors never push, so the lead pushes
`quest/xapiand/postings-fuzz-memory` for the `linux-asan-ubsan` CI check.

## Next

1. When both jobs pass and CI is green: State to ready for review (security lens).

## Blocked on

Nothing; the push for CI is the lead's.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `.campaign/xapiand/journal/postings-fuzz-memory.md`: "ASan, strata check with frame cache" (job 261110-180348-714558 at prospect:10870d9, the before-the-fix record) |
| A2 | Ruled: the postings decoder allocates from no untrusted length (audit under Rulings), so it has no bound to add; what grew is strata's stored check, whose test fails without its fix in stored-check-cache (`test/reader_check_test.cc`, strata:71750940; revert job 261110-182037-74c93c) |
| A3 | 261111-001821-97b359 at prospect:7cb5226 (strata 94480d52): 7 min ASan+UBSan, 0 crashes, RSS 368-388 MiB after 120 s, peak 405 |
| A4 | build-test 261110-232704-7f9c0e at prospect:7cb5226 |

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

Not evidence: 261111-002636-18c28c, A1 rerun at the head, cannot apply its before-the-fix setup; 261110-232721-039d5f timed out in its build under the old 10-minute cap; 261111-000852-ee3f52 ran a clean fuzz but its RSS parse read the summary line and exited 1.

## Result

Pending the fix.

## Rulings I made

- Pin bump in prospect `CMakeLists.txt` (outside the recorded file set fuzz/**, include/**,
  test/**) on the lead's instruction of 2026-11-10; it also brings strata's manifest-log commits
  between 9d4630c and 71750940.
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
