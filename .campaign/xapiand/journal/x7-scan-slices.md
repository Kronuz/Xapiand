# x7-scan-slices: journal

State: waiting on jobs 261112-003707-96568a 261112-003708-c3d2d6 261112-003718-87dd09 261112-003720-34c4cb 261112-003722-18be3b
Quest: .campaign/xapiand/quests/x7-scan-slices.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead's message (row 261111-231025-b3ef28 segfaulted): the crash was the bench harness, not the
scan. `bench()` passed a temporary `query` to `builder_for`, which keeps a reference to it, so
every scan read a dead query; both binaries crashed (the first smoke 261111-230634-53c6e2 printed
no bench line from A, B or A2: its pipeline had no pipefail). The test proper (`host_windows()`)
binds its queries for the loop and was unaffected. Fixed at prospect:ca65df9; the smoke and row
now also require six bench lines per invocation.

Jobs at prospect:ca65df9: build-test 261112-003707-96568a (A1), mutant rerun
261112-003708-c3d2d6 (A1 --fails-without), A/B build 261112-003718-87dd09 (slot x7ss-ab), smoke
261112-003720-34c4cb, row 261112-003722-18be3b (A2: 8 rounds of `bench 150000 21`, A B A2).

## Next

1. From the row: per arm B/A and A2/A over rounds; write docs/perf/scan-slices.md (A3), the
   README row, and fix the mechanism note's rationale sentence with the measured ratio.
2. Commit, push, paste `campaign brief`'s table, State "ready for review".

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- "Per-match work" is a visitor, or a key that runs (`o_.key && o_.k`): with k = 0 the key is never
  called, so that scan keeps 4,096. A descending scan with no key or visitor keeps 4,096.
- The constant `host_window_ordinals = 256` lives in shards.hh (slice.hh is outside the file set);
  the span is chosen once per segment scan, so the per-match loop is unchanged.
- The A/B harness is a `bench` mode of sliced_test (no bench tool uses shards.hh, and the file set
  holds only the test); base is built from the same test source against base's shards.hh, so only
  the header under test differs.
