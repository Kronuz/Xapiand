# flush-within-budget: Indexing stays within its memory budget through the flush

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status flush-within-budget`.

## Outcome

A flush no longer holds its positions spill in memory: strata's flush writes it through
`file_spill` (`section_io.hh`) instead of `chunked_spill` (`index.hh:4559`), so indexing's peak
RSS stays within its node budget plus 10% while the flush trigger stays near the whole budget
(prospect `docs/perf/index-memory.md`: 1.52x at 1M and 1.57x at 5M today, ~284 MiB of the 1M
flush's 403 MiB is that spill).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 Peak RSS at most 1.1x the budget on prospect's keyed 1M single-thread run at the default
  trigger, with the segment count unchanged: one light job at the head (`tools/vm/index_mem.sh`
  from prospect's explore/index-memory, or its equivalent), against the same run at main.
- A2 Segments are byte-identical with and without the change (the spill's destination changes, not
  its bytes): a test or a job comparing section digests.
- A3 A flush that fails writing the spill file (ENOSPC, I/O error) fails the flush and leaves no
  temporary file behind: a test with injected faults.
- A4 The strata suite passes at the head, with CI green on Linux.

## Contracts

None changed: segment bytes and the budget API stay as they are.

## Decisions

Lead: file spill over a lower trigger (a 65-70% trigger meets 1.1x but flushes smaller
segments). If the remaining ~120 MiB of builder scratch keeps the peak above 1.1x, the trigger
reserves that share (`needs_flush`, `index.hh:2410`) as well.

## May land early

Tests.
