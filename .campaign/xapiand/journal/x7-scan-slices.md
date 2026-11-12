# x7-scan-slices: journal

State: waiting on jobs 261111-230601-107439 261111-230602-46c1c9 261111-230624-4bea51 261111-230634-53c6e2 261111-231025-b3ef28
Quest: .campaign/xapiand/quests/x7-scan-slices.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs at prospect:bcd57e0: build-test 261111-230601-107439 (A1), mutant 261111-230602-46c1c9 (A1
--fails-without: span reverted to window_ordinals, expects `CHECK(got.most() <= 256)` to fail),
A/B build 261111-230624-4bea51 (slot x7ss-ab: `build` = head, `build-base` = head's sliced_test
with base prospect:0aa68f4's shards.hh via `-iquote .scratch/base`), strict smoke
261111-230634-53c6e2 (one round of `sliced_test bench 150000 3`, sides A B A2).

## Next

1. Read the row 261111-231025-b3ef28 (strict, `--for A2`, after the smoke: 8 rounds of
   `sliced_test bench 150000 21`, sides A B A2): per arm (plain, count, agg; unsliced, sliced)
   B/A and A2/A over rounds' medians. Cancel and resize it if the smoke shows a problem.
2. Write docs/perf/scan-slices.md (A3) from it; commit; push; paste `campaign brief`'s table.

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
