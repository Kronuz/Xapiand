# x7-scan-slices: journal

State: ready for review
Quest: .campaign/xapiand/quests/x7-scan-slices.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: every criterion has its evidence at prospect:b20e7c5.

## Next

1. Review; then Xapiand's x7-loop-stalls bumps the prospect pin.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261112-003707-96568a (passed at ca65df92a186; ignores docs-only docs/mechanisms/sliced-search.md, docs/perf/README.md, docs/perf/scan-slices.md); fails without: 261111-230602-46c1c9, rerun at ca65df9 as 261112-003708-c3d2d6 |
| A2 | 261112-003722-18be3b (passed at ca65df92a186; ignores docs-only docs/mechanisms/sliced-search.md, docs/perf/README.md, docs/perf/scan-slices.md) |
| A3 | `docs/perf/scan-slices.md` (prospect:b20e7c5) |

Messages: the lead's crash report (row 261111-231025-b3ef28) was a harness bug, fixed at
prospect:ca65df9 (see Rulings); the lead's medians match my recomputation from the row's log.

## Result

prospect:bcd57e0: `exhaustive_search::scan` checks its slice every 256 ordinals
(`host_window_ordinals`, shards.hh) with a visitor or a running key, 4,096 otherwise.
`sliced_test` `host_windows()` proves at most 256 host calls per resume with identical hits,
counts and aggregates; the mutant fails it. Row 261112-003722-18be3b (strict, 8 rounds A B A2,
150,000 docs, 62,440 matches): no host work 1.008 unsliced, 1.015 sliced (A/A 0.985, 1.006);
aggregation visitor 1.020, 0.988; the cheapest visitor 1.056 unsliced, 1.012 sliced. The 5.6% is
not the extra window exits (about 550 per scan, about 0.4% at 10 ns each; the sliced arm pays
them plus a TSC read and shows 1.2%), so placement or noise; not rerun (lead's ruling).
Docs: docs/perf/scan-slices.md, docs/perf/README.md row, docs/mechanisms/sliced-search.md.
Open: the Xapiand turn bound (about 2.3 ms to 0.14 ms) is derived, measured by x7-loop-stalls
after the pin bump.

## Rulings I made

- "Per-match work" is a visitor, or a key that runs (`o_.key && o_.k`): with k = 0 the key is never
  called, so that scan keeps 4,096. A descending scan with no key or visitor keeps 4,096.
- The constant `host_window_ordinals = 256` lives in shards.hh (slice.hh is outside the file set);
  the span is chosen once per segment scan, so the per-match loop is unchanged.
- The A/B harness is a `bench` mode of sliced_test (no bench tool uses shards.hh, and the file set
  holds only the test); base is built from the same test source against base's shards.hh, so only
  the header under test differs.
- The first row crashed because `bench()` passed a temporary `query` to `builder_for`, which keeps
  a reference; both binaries crashed and the first smoke hid it (no pipefail). Fixed at
  prospect:ca65df9 (a named local); smoke and row now require six bench lines per run.
