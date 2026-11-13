# x7-loop-stalls review: design

Commit: Xapiand:79904750c0cf99e41b41c82eba2ca247250595d0
Verdict: CHANGES
Date: 2026-11-11

Subject: the section "Design: RESTORE's prepare as an index chore" of `docs/perf/x7-loop-stalls.md`, the amendment to
phase7c.md D4 and D12 that the quest's 11-11 ruling sends to one design review. Code read at this commit (exported with
`git show`; the worktree has since moved to Xapiand:bdb333379); no GPL sources read.

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- The note's RESTORE profile shares (x7-gate job 261111-171048-c39d8d, as tabled in the note) for the arithmetic in
  D6. No criterion of this quest has evidence yet (`campaign status x7-loop-stalls`), and none is needed for a design
  ruling.

## What I checked myself

- Probe job 261111-232328-9fc12d: `.scratch/review-x7-loop-stalls-design/raw_probe.sh` against x7-gate's head binary
  (`~/.scratch/x7-loop-stalls/49ebb4481/xapiand`, master's `src/`). One RESTORE of
  `[{_id:a, first:1}, {_id:a, _op_type:update, second:2}, {_id:b, first:1}, {_id:b, _op_type:patch, ...}]`, then GET
  of each id. Result (exit 0): `GET a` is `{"_id":"a","first":1,"_version":2,"second":2}`: today the update reads the earlier object of its own stream and merges onto it. (The patch half used a malformed patch body and answered 400; the update half settles the question.)
- Code: the questions' answers below, each with its `file:line` at this commit.

The lead's questions, answered:

1. **Single WorkPool.** Kept. The prepare runs in the RESTORE's own `IndexChores` group (`search_views.cc:1668`), the
   `flush` class, on the host pool; no thread, pool or queue is added, and D12 already names RESTORE batches as
   parse-ahead chores. Only the size of each item needs a rule (D5).
2. **Ordering.** Applies stay in input order on the loop, so `_version` (set in the turn by `finalise_write`,
   `shard.cc:107-149`) and new numeric `_id`s (numbered by the shard, `NEW_NUMERIC_KEY`, `schema.cc:2805-2817`) are
   assigned in input order as today. Two orderings change, and both are blocking: reads of the current document
   (D1) and schema updates (D2). Concurrent writers from other requests interleave between RESTORE's objects today
   too (a checkpoint per object, `handler.cc:2362-2378`); the read-ahead only widens that window for D1's objects.
3. **Lifetime and threads.** The pure part touches nothing loop-bound once the script and the reads stay on the loop:
   `index_fields` and below are synchronous (no `co_await`, no manager or shard call outside `Schema::index`'s id
   and script blocks, `schema.cc:2708-2998`); stemmers are `thread_local` (`generate_terms.cc:504-507`); stop words
   are read-only after startup; cached schemas are `lock()`ed (fully filled) before they are shared
   (`schemas_lru.cc:267` and the other `lock()` sites), so concurrent const reads do not run MsgPack's lazy `_fill`.
   `engine::Document` is a plain value (`document.h:75-166`). The ownership text needs fixing (D3, D4).
4. **Failure and cancellation.** Consistent with today: the request's budget carries its cancel token
   (`search_views.cc:3166-3167`), what applied stays and the closing commit runs on every exit
   (`search_views.cc:1750-1763`), and `task::when_all` rethrows only after every child ends, so a parse error never
   orphans a prepare in flight. One widening (D7).
5. **The open point.** Leave `_version` and a new numeric `_id` out of the record (the alternative), with the costs
   listed in D6; reasons there.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D1 | yes | defect | An object whose prepare reads the current document no longer sees the earlier objects of its own stream. `_op_type` `patch`, `update` and `merge` read the stored document before preparing (`handler.cc:1136-1180`), and a schema script reads `_old_doc` (`call_script`, `handler.cc:340-360`, called from `schema.cc:2912`). Both go through `find_docid_term` -> `engine_for_read` -> `ShardEndpoint::fresh`, which refreshes a dirty index first (`handler.cc:1752-1775`, `shard.cc:1333-1338`, `pool.cc:362-371`), so today object j sees every earlier object's apply. The design does these reads on the loop before posting the sub-batch, and prepares the next batch while this one applies, so j's read can precede the apply of an earlier object with the same id: the update applies to a stale base (the earlier write is lost) and the script sees a stale `_old_doc`. | `handler.cc:1136-1180`, `pool.cc:362-371`; probe 261111-232328-9fc12d | State a barrier: an object that reads the current document (a non-`index` op, or any object when the schema has a script) is read and prepared only after every earlier object of the stream has applied; plain `index` objects with no script keep the parallel path. Test in `tests/test_reactor_requests.cc`: the probe's stream with the update at the start of a sub-batch and of a batch, GET shows both fields; the mutant without the barrier fails. |
| D2 | yes | defect | Schema updates commit in completion order, not input order. "Two sub-batches that change the schema at once apply their updates one at a time on the loop, and the second re-prepares": the second to return loses `update_schema`'s compare-and-swap (`handler.cc:1543-1553`), whichever its input position. A new field's type comes from its first indexed occurrence, so a later object can fix the type that an earlier object fixes today; the earlier object then gets a type error it does not get today (or the reverse), and the final schema varies from run to run. RESTORE into a new index hits it on its first batch, where every sub-batch changes the schema. | `handler.cc:389-399`, `handler.cc:1543-1553`, `schema.cc:2730-2738` | Commit schema changes in input order: sub-batch k offers its change only after every earlier sub-batch is accepted, and when an earlier one changed the schema, k re-prepares from its first changed object against the new snapshot. Test: a stream where field `x` first appears as an integer in sub-batch 0 and as a string in sub-batch 1, with sub-batch 1's chore made to finish first; the schema and per-object results equal the sequential run's. |
| D3 | no | gap | The lifetime paragraph describes a mechanism `IndexChores::run` lacks. Its `ChoreAwaiter` is the `WorkItem`, lives in the awaiting frame and resumes unconditionally (`pool.cc:70-140`); a `shared_ptr` state does not make destroying a suspended frame safe while its chore runs, since the item itself would dangle. Safety comes from `cancel_all` draining running chores before tasks are destroyed (`pool.h:100-103`, `pool.cc:177-213`) and from D3 R6.3 (frames end by completing). | `pool.cc:70-140`, `search_views.cc:2968-2985` | Keep the sub-batch state in the awaiting frame, as `Request::parse_ahead` does (one mechanism), and drop "resumes the task only if it still waits"; or specify the new awaiter and why parse-ahead does not need it. |
| D4 | no | nit | "A pure part over `(const Schema&, object, Data)`" and "`shared_ptr<const Schema>`, immutable" cannot hold: `Schema::index` mutates its Schema (`map_values`, `specification`, `mut_schema`, `schema.cc:2722-2735`, `:2992`). The shareable snapshot is the locked `shared_ptr<const MsgPack>` (`schema.h:566`); each chore builds its own `Schema` over it and returns its modified schema. | `schema.cc:2722-2735` | Reword. |
| D5 | no | gap | Item size. The RESTORE group's lanes have no width cap (`GroupState::width` 0, threadpool `work_pool.hh` at the pinned 63e03a5) and an item runs to completion, so `workers` sub-batches of a large batch hold every non-reserve worker for a whole sub-batch, delaying other indexes' commits and blob writes behind them (DRR accounts service but cannot preempt). | threadpool `work_pool.hh:411-470`, `:1216-1218` | Size items by work (for example about 1 ms of objects each) rather than `batch / workers`; the parse chore then interleaves too. |
| D6 | no | gap | The open point: choose the alternative (no `_version` or new numeric `_id` in the record). From the note's own shares, after the chore the loop's time per object is the shard's turn plus loop-side bookkeeping. With compress-once as ruled, the turn keeps `finalise_write` less its decompress (34.3 - 4.2, about 30% of today's loop time) plus the rest of `Shard::replace` (45.1 - 34.3, about 11%): about 41% (about 50% in arms 3 and 6) of 0.5 ms, 0.2-0.25 ms per object, about 4,000-5,000 docs/s against pre-7c's 7,039-7,866, about 0.55-0.7x. T12 would still miss and fix (c) would become necessary. With the alternative the turn keeps about 11%, which leaves room above 1.0x. This is an estimate from profile shares, not a measurement; A3's job decides. It also stores the version once, in its slot, instead of twice. Its costs, which the design must list and test: `_dump_document` builds a dump from the record alone (`handler.cc:858-891`), so a dump would lose a new numeric `_id` (a restore would renumber the documents) and `_version` unless `dump_page` also returns the id and version slots; `call_script`'s `_old_doc` and the bases of patch, update and merge come from `Data::get_obj` (`handler.cc:357-359`, `:1150`, `:1170`); retrieve and search already fill both (`search_views.cc:1890-1898`, `:2049-2053`). | note's RESTORE table; `handler.cc:858-891` | Owner's call, since it changes what the record holds. If chosen: fill at every record reader above, and test a dump and restore round trip of a numeric-id index (ids kept) plus `_version` in GET, search and dump. Keeping compress-once is acceptable if fix (c) is planned with it. |
| D7 | no | nit | "A chore that throws fails its sub-batch's objects with 500" widens a failure: today `co_catch_http_errors` wraps each object's prepare (`handler.cc:2251-2266`, `:2299-2301`), so any throw is that object's result alone. | `handler.cc:2299-2301` | Catch per object inside the chore. |
| D8 | no | nit | "Shard placement for new numeric ids (it pins shards for their counts)" is inaccurate. RESTORE passes `seq = idx + 1` (`handler.cc:2300`), so uuid and keyword placement is `seq % n_shards` plus `Endpoint::is_active()` (an atomic read, `endpoint.cc:355-359`), and a new numeric id takes `NEW_NUMERIC_KEY` and pins nothing (`schema.cc:2805-2817`); only the `seq == 0` count scan pins. For RESTORE the only loop-bound step inside `Schema::index` is the script. | `schema.cc:2760-2817` | Reword; the split can then be "all of `Schema::index` in the chore unless the schema has a script". |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit. A
finding that would remove or narrow user-visible behavior offers options, including keeping it,
and is never blocking.

## Not run

- No timing or profile; D6's numbers are arithmetic on the note's shares.
- The built chore code (none at this commit); TSan over the handoff is the correctness review's, with the code.
- Whether a dump carries its schema so that a dump restore never changes the schema (D2 then bites only hand-made
  streams); not checked.
- Teardown when `cancel_all` times out with a chore running: pre-existing for every chore, not this design's.
