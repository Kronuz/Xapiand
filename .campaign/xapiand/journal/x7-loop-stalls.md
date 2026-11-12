# x7-loop-stalls: journal

State: ready for review
Quest: .campaign/xapiand/quests/x7-loop-stalls.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 done: `docs/perf/x7-loop-stalls.md` CURRENT (profiles, ranked fixes, the RESTORE chore design revised for review
D1-D8).

Built, head Xapiand:6ff9db94a:
- Fix 1 (Xapiand:00cd38f39): `search_view` checkpoints the budget before each hit.
- Compress-once (Xapiand:2f8145379): the prepare stores the record raw; `finalise_write` compresses it once.
- RESTORE prepare as chores (Xapiand:a2dffbd5e, 0c99ca2bd, 6f417c99c): `DocIndexer::index` (`handler.cc`) runs rounds:
  `fetch` (schema snapshot, on the loop), `pull_round` (one flush chore per non-reserve worker, each pulling objects
  in input order against its own `Schema`, about 1 ms or to its first schema change; the only part overlapping the
  apply), `accept` (objects up to the first change; offers that change with no apply in flight, since a task holds
  one turn at most). Barriers (`reads_stored`: non-`index` `_op_type`, root `_script`/`_schema`; any object while the
  schema has a script) run on the loop after the run before them applies; each batch's last run applies before
  `index` returns. `DocIndexer` takes the RESTORE's `IndexChores` (`restore_database_view`); the CLI `--restore` passes
  `engine::inline_chores()`. `index_one` is gone. phase7c.md D4 (`flush` row) and D12 amended (6f417c99c).

Evidence so far (all at older heads; superseded by the jobs below):
- Vacuous: build-test 232734-59ff14 and 235649-63420f (the old check had no `-DBUILD_TESTS=ON`; ctest ran nothing).
  The quest's check is now x7-gate's form (tests, asserts, async and option checks): `campaign verify`.
- test_loop_yield with asserts at 0c99ca2bd (b71dc6): every check passed but the schema-order test's whole-schema
  comparison, which also failed under the all-barrier mutant (882c06), so it was a test bug: the test now compares
  `x`'s entry and checks it is an integer (6ff9db94a). Seen at 0c99ca2bd: the update tests pass (GET shows both
  fields); per-object results equal the sequential run (the string fails 400 "Format invalid for integer type");
  off-loop share 34% at the fix, 60% under the all-barrier mutant (threshold now 45%).
- Mutants that failed as required at older heads: hits checkpoint (af5357, 0ecdb6, 55273a), compress-once
  (c4c72b, 037ffb), D1 in a batch (0e58e3).
- At a2dffbd5e the D2 mutant's asserts build hit "a task holds one index's turn at most" (offer beside the apply):
  fixed by the fetch/pull/accept split (0c99ca2bd).

At 6ff9db94a every mutant failed at its line as required (478825, 05438d, 0c957d, 5c5bae, 86670c, b2314b) and the
real suite (b6976b) ran 38/38 green but failed `async_check --strict`: one unclassified `sync_wait` at handler.cc's
`prepare_plain`. Resolved (Xapiand:1faf61abb): `Schema::index` there is CPU only, so the chore keeps a blocking call,
now `engine::sync_wait` (the one guarded bridge, as shard.cc's open chore does) instead of a raw `task::sync_wait`,
with an allowlist row naming why. Prospect pin moved to b20e7c5 (Xapiand:232b81cba); strata pin unchanged (5d742b2).

Jobs at 232b81cba: build-test 261112-011615-3ea00b; timing build (light, quest slot) 261112-012852-295929 (75a8df lost: submitted from .scratch; bb7a01 failed: lua.org download in a fresh checkout) copies the fix's
binary to `~/.scratch/x7-loop-stalls/0072ba93e/xapiand` (done: prospect b20e7c5, strata 5d742b24, shasum 5e9c9693); A2 (T4+T5, exclusive) 261112-012855-b34e3c; A3 (T10 + T12
loadtest and profile_bulk, exclusive) 261112-012857-a77bf8. Pre-7c binary: `~/.scratch/x7-loop-stalls/142b2ca77/xapiand`,
the same bytes as x7-gate's build-pre (shasum 787b7174). Targets: T4 every HTTP loop's p99 < 1,024 us; T5 loaded light
p99 near pre's 8.93 ms; T10 flood light-GET p99 about 20 ms; T12 RESTORE docs/s and QPS >= 1.0x pre-7c.

Older jobs at 6ff9db94a (`--for A4`; each mutant needs exit 1 and its check's line in test_loop_yield.cc):
build-test (the real suite) 261112-002508-b6976b; mutants: hits checkpoint 478825 (:320), compress-once 05438d
(:341), D1 in a batch 0c957d (:415), D1 batch boundary 5c5bae (:414, first:1 lost), D2 86670c (:447), every object
a barrier b2314b (:383). 261112-000846-ac7a9e (build-test at a29bb36e6, a journal commit) is not mine to rely on.

## Next

1. Review (the lead's to launch; TSan over the chore handoff). Profile E3 (261112-020117-4c8576, no criterion) names
   the turns that still hold the loops 2.5-5 ms; read it into the note's "What still misses".
2. T12 short (0.57-0.59x): fix (c), the apply per shard in parallel, is the lead's call (a new step or quest).
3. (done) Suite, pin, A2/A3.

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

- Owner (D6): leave `_version` and a new numeric `_id` out of the stored record; not built.
- Lead: `harness/async_allowlist.tsv` is outside the file set; its one row (handler.cc `sync_wait schema\.index`) went
  in on the lead's step-1 instruction. Widen the set to it, or move the row.
- Lead: prospect b20e7c5 pins strata 94480d5 (the manifest log), newer than Xapiand's 5d742b2, so `pins_check.py`'s A3
  (Xapiand pins the newest any library pins) fails and CMakeLists' line 456 comment is false until a strata bump.
  Prospect's library change is shards.hh alone and builds on 5d742b2. Recommendation: a separate strata-bump quest
  (the manifest-log port is about 1,400 lines of index.hh), not this one.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | [artifact] `docs/perf/x7-loop-stalls.md` (profiles E1a 18ce81, E2 793974; causes, ranked fixes, the chore design) |
| A2 | 261112-012855-b34e3c: T4 MISS (every HTTP loop p99 2.56-5.12 ms vs 1,024 us; pre 640-768 us); T5 MISS on its bound (light p99 27.1/22.7 ms vs 10.2/8.9 ms), below pre-7c (0.065x) |
| A3 | 261112-012857-a77bf8: T10 MISS (flood light-GET p99 45.4 ms vs about 20); T12 RESTORE MISS 0.571x (loadtest) / 0.594x (profile_bulk); QPS PASS 2.69x |
| A4 | 261112-011615-3ea00b; fails without: 261111-230628-74a222, 261111-232735-af5357, 261111-232738-c4c72b, 261111-235701-0ecdb6, 261111-235704-037ffb, 261111-235708-0e58e3, 261112-000958-55273a, 261112-002521-478825, 261112-002525-05438d, 261112-002529-0c957d, 261112-002533-5c5bae, 261112-002537-86670c, 261112-002541-b2314b |

## Result

Changed: the hit loop yields (fix 1), the record is compressed once, RESTORE's prepare runs as flush chores on the
WorkPool with the apply in the shard's turn (phase7c D4/D12 amended), prospect pinned at b20e7c5 (256-ordinal scan
window with host work). Measured at Xapiand:0072ba93e against pre-7c 142b2ca77 (table and causes:
`docs/perf/x7-loop-stalls.md`, "At the fix"): the loops' long turns are gone (median loop p99.9 57 to 6-7 ms, max 69 to
20-22 ms) at 5.6-6.0x pre-7c's heavy throughput, light p99 under load 0.065x pre-7c, RESTORE 0.29x to 0.57-0.59x, QPS
2.69x. Open: T4/T5 still 2.5-5x over the edge (E3 names the turns; fix 3 next), T10 is the Lua per-call cap
(outside this file set), T12 needs fix (c), the parallel apply (cores 2.6 vs pre's 5.9 at 0.76x the CPU per document).

## Rulings I made

- D2 rule built conservatively: every object after an accepted schema change is prepared again (not only the
  changed ones), so a round equals the sequential semantics without arguing which prefixes stay valid.
- Chores pull from one cursor (not fixed sub-batches): a change wastes about one object per worker, so a new index
  costs one round per change; the 1 ms slice bounds a chore (D5).
- The chore runs `Schema::index` through `engine::sync_wait` and stays a blocking call: chores are
  `std::function<void()>`, and on this path `Schema::index` never suspends (its only awaits are `call_script`, a
  barrier since a root `_script` comes only from the object or the schema, and `lock_shard`, only when `seq` is 0;
  `seq` is idx+1). Classified `allow` in the async allowlist rather than reshaping the chore API.
- Tests live in `tests/test_loop_yield.cc`, which has the in-process node and HTTP client; the reviewer named
  `test_reactor_requests.cc`, which has neither a manager nor schemas.
- msg (lead, handler.h and phase7c.md in the file set): `DocIndexer` takes `engine::Chores&` (the RESTORE's
  `IndexChores`; inline for the CLI restore); `index_one` dropped; D4/D12 amended, touching only that text.
- The CLI restore uses `engine::inline_chores()`: it runs under `sync_wait` on the main thread with no reactor.
- Width of a round: the pool's threads less the reserve, read from the manager's pool (1 without one).
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
