# restore-free-off-loop: Free a RESTORE chunk's results without holding the loop

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status restore-free-off-loop`.

## Outcome

At the end of each RESTORE chunk, freeing the chunk's per-document results (`~DocIndexer`, src/server/search_views.cc:1774; `_results`, src/database/handler.h:361) no longer holds the reactor loop for 10-25 ms. Fix the cause: the results live in one arena or flat buffer released at once, or are released off the loop. Slicing the free with budget checkpoints (harness/restore_stall_slice_results.patch) is the fallback only if the cause cannot be removed, with the reason recorded.

## Acceptance criteria

- A1 During a RESTORE stream, trivial GET p99 is within 2x of the A/A idle p99 (before: 22.4-24.4 ms against 2.2 ms) and no loop turn exceeds 5 ms in the RESTORE phase, macOS under `quiet strict`, ABBA against master: a job
- A2 RESTORE throughput is within the A/A band of master (about 6,700 docs/s on the probe corpus): the same job
- A3 RESTORE results are unchanged: the docs E2E plus the restore tests pass: the `e2e` and `build-test` checks
- A4 [artifact] The measurement note: `docs/perf/restore-stall.md`

## Decisions

- Lead, 11-16: the cause is the one-turn free (81% macOS madvise); fix it at the allocation, not by spreading the free. Measure Linux once if a Linux host is available through CI; otherwise say so.

## May land early

None.
