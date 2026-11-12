# x7-loop-stalls: journal

State: waiting on jobs 261111-225130-793974
Quest: .campaign/xapiand/quests/x7-loop-stalls.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 (profile note, `docs/perf/x7-loop-stalls.md`; file set is only that note). Binaries: x7-gate's build job
261111-171014-19268b, copied to `$HOME/.scratch/x7-loop-stalls/49ebb4481/xapiand` (master's src, unchanged after the
merge) and `.../142b2ca77/xapiand` (pre-7c). Jobs run at x7-gate head Xapiand:234fe089a for its harness.

- RESTORE section done (Xapiand:f1e29b97e), from x7-gate's t12b samples.
- Loop section written from E1a 261111-221539-18ce81: loops at 43-49% CPU (not saturated), lag from long single turns.
  Cause: (1) `search_view`'s 100-hit loop plus response, no suspension point, about 5 ms CPU per heavy search;
  (2) `exhaustive_search::scan`'s 4,096-ordinal window with the aggregation visitor per match, about 0.56 us per match,
  about 2.3 ms per window. Shares: scan+spy 48.4%, hit loop 24.2% + `Shard::get` 19.4% (stored frame decode 14.4%).
- E2 261111-225130-793974 (exclusive, 9 min, no --for: A1 is [artifact]): each heavy kind alone at 16 search clients,
  RESTORE with one text client; its lag per kind measures turn length directly (the note's `<!-- E2 -->` marker).
  Script `$HOME/.scratch/x7-loop-stalls/e2.sh`, wrapper `.../drive.py`.

## Next

1. Fold E2's table into the note at `<!-- E2 -->` (per kind: lag p50/p99 median and max loop); if aggregations alone
   stay over 1 ms and the 100-hit kinds are higher, the cause stands as written; then set the note CURRENT.
2. Once the lead widens the file set: fix 1 (hit-loop checkpoint) with its test and mutant; fix 2 in prospect; then A2/A3
   jobs (x7-gate's procedure, exclusive, at most 10 min each).

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

- Lead: the file set for the loop fix. Exact files: `src/server/search_views.cc`; `tests/test_reactor_requests.cc` (or a
  new `tests/test_hit_slicing.cc` plus its name in `CMakeLists.txt`'s test list) for fix 1's test; `CMakeLists.txt` for the
  prospect pin; fix 2 is a commit in the prospect repository (`include/prospect/shards.hh`, `test/sliced_test.cc`):
  who makes it, this quest or a prospect quest? Fix 3 only if needed: `src/engine/strata/shard.cc`,
  `src/engine/strata/doc_view.h`, `src/engine/strata/doc_view.cc`, `src/aggregations/bucket.h`.
- Lead: RESTORE's prepare-as-chore amendment needs its design review before I build it (files then:
  `docs/design/phase7c.md`, `src/engine/chores.h`, `src/database/handler.cc`, `src/database/handler.h`,
  `src/database/shard.cc`). Fix (b), compress once at finalise, needs only `src/database/shard.cc` and
  `src/database/handler.cc` and can go first.

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

- E2 runs without `--for A1`: `campaign` refuses a job for an [artifact] criterion; it is evidence cited in the note.
- Shares come from the sample; durations from CPU per request and the lag probe, since `sample` stops the process
  (2.75 ms per sample) and inflates busy wall.
