# x7-loop-stalls: journal

State: waiting on jobs 261111-232734-59ff14 261111-232735-af5357 261111-232738-c4c72b
Quest: .campaign/xapiand/quests/x7-loop-stalls.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 done: `docs/perf/x7-loop-stalls.md` CURRENT (RESTORE section, loop section from E1a 261111-221539-18ce81, E2
261111-225130-793974's per-kind table, ranked fixes, RESTORE-prepare-as-chore design for the lead's review).
Master merged (Xapiand:0f139cd54; master's `src/` equals the profiled build's).

Fix 1 built (Xapiand:00cd38f39): `search_view` awaits `request.budget->checkpoint()` before each hit.
Test `tests/test_loop_yield.cc` (registered in `CMakeLists.txt`): an in-process node with one HTTP loop, 5,000 docs,
a `_limit: 5000` search; checks the HTTP loop's probe's worst wake in the window is under a third of the request.
Jobs: build-test 261111-230627-642de6 (`--for A4`), mutant 261111-230628-74a222 (`--for A4 --fails-without`: deletes
the checkpoint line, builds the test, exits 0 when it fails).

Compress-once built (Xapiand:2f8145379): `Data::set_obj(obj, bool compress = true)`; `prepare` stores the record
raw, `finalise_write` rewrites it once compressed when modified or raw. `Locator::compressions()` counts zstd record
compressions. Test `test_a_restored_record_is_compressed_once_and_reads_back`: 200 restored records, 200
compressions; three records read back with `n`, `text`, `_id`, `_version` 1.
Fix-1 mutant 261111-230628-74a222 is not evidence: the test process died of SIGUSR2 (the node's stall watchdog
collects stacks with it; the test had no handler), not of a failed check. Fixed (Xapiand:102d6609e: the handler, as
main() installs it); the mutants now require exit 1 (checks failed).
Jobs at 102d6609e: build-test 261111-232734-59ff14 (`--for A4`), fix-1 mutant 261111-232735-af5357, compress-once
mutant 261111-232738-c4c72b (both `--for A4 --fails-without`). Cancelled as superseded: 642de6, 11dc15, 7f715c.

E2 (no `--for`): 100-hit kinds p99 lag 12.3-14.3 ms, phrase 7.2-8.2, `_terms age` 6.1-7.2, `_values state` 3.1,
RESTORE + one text client 0.38 ms median loop, 10.2 ms worst.

## Next

1. Read the two jobs. If the test passes at the fix and fails in the mutant, A4's fix-1 half stands; if it flakes,
   widen the margin (more hits) rather than loosen the ratio.
2. After x7-scan-slices lands: bump the prospect pin in `CMakeLists.txt`, then A2 (T4/T5) and A3 (T10, T12) jobs,
   x7-gate's procedure, exclusive, at most 10 min each.
3. Compress-once (blocked, below) and the chore after its design review.

## Plan (the fix)

Loops: (1) `search_view` awaits `request.budget->checkpoint()` between hits (`src/server/search_views.cc`, its only
per-hit loop); test: N hits on a zero-length budget resume about N times, mutant without it once. (2) prospect's
`exhaustive_search::scan` checks the slice every 256 ordinals when a visitor or key runs (prospect `shards.hh` +
`test/sliced_test.cc`; then the pin in `CMakeLists.txt`). (3) Only if A2 stays short: reuse a stored frame buffer across
a request's hits (`src/engine/strata/shard.cc`, `doc_view.{h,cc}`), format `_terms` integer keys at render
(`src/aggregations/bucket.h`).
RESTORE (lead's ruling): (a) prepare as an index chore on the WorkPool after a design review of the phase7c.md
amendment; (b) compress once at finalise (`src/database/shard.cc` `finalise_write`, `src/database/handler.cc`
`prepare`/`DocIndexer`); (c) parallel apply only if T12 stays short.

## Blocked on

- Lead: the design review of the note's "Design: RESTORE's prepare as an index chore" section (running); not built.
- x7-scan-slices landing, then the prospect pin bump and A2/A3.

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

- msg (lead, 11-11): E1 split; the mix sample first. RESTORE ruling recorded in the quest's Decisions (prepare as an
  index chore on the one WorkPool; compress once at finalise; parallel apply only if T12 stays short).

- Lead's msg (11-11, data.h/data.cc): compress-once built with `Data::set_obj(obj, compress)` and a counted test; the
  record-content alternative is in the design section as the reviewer's question only.
- `finalise_write` also rewrites a raw record it did not modify, so no path that prepares raw stores it raw; a record
  under 128 bytes stays raw after its compress attempt and is rewritten on its next write (cheap).
- Lead's msg (11-11, rulings 1-4): file set widened; fix 2 is x7-scan-slices (prospect untouched); design written
  in the note; E2 folded in; A2/A3 wait for fix 1 and the pin.
- Fix 1's checkpoint goes before each hit (not after): a hit's work is never split, and the first hit runs in the
  search's last slice when it has time left.
- The test measures the loop's own probe (bucket upper edges) against the request's wall time, with a 3x margin; the
  mutant shows it fails without the checkpoint.
- E2 runs without `--for A1`: `campaign` refuses a job for an [artifact] criterion; it is evidence cited in the note.
- Shares come from the sample; durations from CPU per request and the lag probe, since `sample` stops the process
  (2.75 ms per sample) and inflates busy wall.
