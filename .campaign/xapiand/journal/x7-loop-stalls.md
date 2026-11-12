# x7-loop-stalls: journal

State: waiting on jobs 261112-000934-d70ff9 261112-000950-b71dc6 261112-000958-55273a 261112-001007-d81b98 261112-001015-862667 261112-001024-e9466d 261112-001033-56b28b 261112-001042-92529a
Quest: .campaign/xapiand/quests/x7-loop-stalls.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 done: `docs/perf/x7-loop-stalls.md` CURRENT.

Jobs at Xapiand:102d6609e all green: build-test 261111-232734-59ff14, fix-1 mutant 261111-232735-af5357 (fails at
:237, the hits check), compress-once mutant 261111-232738-c4c72b (fails at :258, 400 compressions).

Design review (CHANGES, D1-D8) folded into the note's design section (Xapiand:9f2e95cb0), then built
(Xapiand:a2dffbd5e): `DocIndexer::index` prepares plain objects in rounds of flush chores (one per non-reserve
worker, each pulling objects in input order against its own `Schema` over one snapshot, about 1 ms each or to the
first schema change); the loop accepts up to the first change, offers it, re-prepares the rest on the new schema, and
applies in input order while the next round prepares. Barriers (D1): non-`index` `_op_type`, root `_script` or
`_schema`, or a schema with a script, prepared on the loop after the run before them applies; each batch's last run
applies before `index` returns. Shared helpers with `DatabaseHandler::prepare`: `document_id_of`, `take_op_type`,
`finish_prepared`, `offer_schema`.
Tests in `tests/test_loop_yield.cc` (the node harness; `test_reactor_requests.cc` has no manager or schemas):
off-loop CPU share (<60% of process), update after a run and at a batch boundary, schema order vs a sequential restore.
Jobs at a2dffbd5e: build-test 261111-235649-63420f green; mutants hits 0ecdb6, compress 037ffb, D1 in a batch
0e58e3 passed (failed as required). D1 boundary 2d5987: the mutant aborted the run in the compress-once test (`at` on
a missing doc), exit 134. D2 52e56d: the asserts build hit "a task holds one index's turn at most": the round's
schema offer (a metadata write in a turn) ran beside the apply's turn. Fixed at Xapiand:0c99ca2bd: a round is fetch,
chores (the only part overlapping the apply), then accept, which offers with no apply in flight; the tests read a
field only when the reply has it. Off-loop share at the fix: loop 738 ms of the process's 2,148 ms CPU (34%).
Jobs at 0c99ca2bd (`--for A4`): build-test 261112-000934-d70ff9; test_loop_yield with asserts b71dc6; mutants
(exit 1 and the named check's line): hits 55273a (:304), compress-once d81b98 (:325), D1 in a batch 862667 (:399), D1
boundary e9466d (:398), D2 56b28b (:431), every object a barrier 92529a (:367).

## Next

1. Read the a2dffbd5e jobs; fix anything red (the off-loop threshold and the schema-order item equality are the
   untested guesses).
2. After x7-scan-slices lands: bump the prospect pin in `CMakeLists.txt`, then A2 (T4/T5) and A3 (T10, T12) jobs,
   x7-gate's procedure, exclusive, at most 10 min each.
3. Correctness review of the chore code (TSan over the handoff is the reviewer's).

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

- Lead (files outside the set, not blocking the build): `src/database/handler.h`, to give `DocIndexer` the
  RESTORE's own `IndexChores` (today each `index()` call makes a group of its own) and drop `index_one`'s stale
  declaration; `docs/design/phase7c.md`, the D4 `flush` row ("RESTORE prepare") and D12, per the note's design.
- Owner (D6): leave `_version` and a new numeric `_id` out of the stored record; not built.
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

- D2 rule built conservatively: every object after an accepted schema change is prepared again (not only the
  changed ones), so a round equals the sequential semantics without arguing which prefixes stay valid.
- Chores pull from one cursor (not fixed sub-batches): a change wastes about one object per worker, so a new index
  costs one round per change; the 1 ms slice bounds a chore (D5).
- The chore runs `Schema::index` through `task::sync_wait`: with barriers for scripts and `seq` never 0 it completes
  without suspending.
- Tests live in `tests/test_loop_yield.cc`, which has the in-process node and HTTP client; the reviewer named
  `test_reactor_requests.cc`, which has neither a manager nor schemas.
- The quest's check builds without asserts; an asserts run of test_loop_yield is queued with the mutants, since the
  turn rule only aborts there.
- The batch-boundary mutant drops the end-of-batch apply: no prepare runs ahead across batches, so that apply is the
  rule at a boundary.

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
