# offload-bridge (engine-core CR1, CR2; pool CR5)

Branch `fix/offload-bridge` from Xapiand master `2726fd225` (local only, never pushed). Ruling:
`docs/reviews/engine-offload-architect.md`, conditions C1 to C6 (C7 is step 7c). Reactor marker:
Kronuz/reactor `feat/on-reactor-marker` `d381e4d` (pushed; the lead lands it). `CMakeLists.txt` pins
reactor at `d381e4d` while developing; the lead repins after landing reactor.

## State

In progress: C1 to C6 written; verification below.

## D13 as amended (C1)

The rule, for the lead to write into D13 (`strata/docs/design/xapiand-on-strata.md:224-235`; strata
is outside this branch):

> No reactor loop thread blocks on database work: no `sync_wait`, no `DatabasePool` checkout or
> cleanup, no shard open, commit, or close. A blocking caller runs on an offload worker (the serving
> reactor's `pool()`, or an existing named thread: DP, DI, the committer, WL, the cleanup thread) and
> the engine call runs inline on it. A coroutine caller on a reactor `co_await`s the task and never
> `sync_wait`s (from 7c). No thread waits, by `sync_wait` or otherwise, on a pool it is a thread of.
> Xapiand enforces it with `engine::check_may_block` (`src/engine/sync.h`).

## Done criteria

- [ ] C1 The rule in D13's text (above; the lead applies it in strata).
- [ ] C2 One bridge, one guard: `git grep -n 'task::sync_wait' src` lists only `src/engine/sync.h`;
  `engine::sync_wait`, `DatabasePool::checkout` (both) and `DatabasePool::cleanup` call
  `engine::check_may_block`: not inside a reactor's `run()` (`reactor::inside_run()`), and
  `task::current()` empty. Debug (asserts) aborts; release counts and logs once per site.
- [ ] C3 FLUSH offloaded (`search_service.cc`), both comments corrected.
- [ ] C4 The restore view finishes and drops `request.indexer` on its worker on every exit.
- [ ] C5 The control-plane reactor has one worker: setup_node's database part and the dispatched
  commands (new_leader/load_nodes, the election's `lock_shard`) run there in order; the state change
  resumes on the loop; a setup failure is rethrown on the loop; `join()` stops and joins the worker
  before the database pool finishes.
- [ ] C6 `Offload`, `set_offload`, `XAPIAND_ENGINE_OFFLOAD_THREADS` removed; `Hop` takes a
  caller-chosen `task::scheduler_ref`; `test_engine_shard`'s pool case uses one.
- [ ] The guard fires in a test that calls a database path from a reactor thread, and the test fails
  with the check removed (revert check).
- [ ] Docs E2E (`docs_to_postman.py` + newman) as on master, guard armed (Debug ASan build).
- [ ] 16-client hammer and `uafhunt.sh` clean a few rounds, guard armed.
- [ ] TSan on the pool tests.
- [ ] Q1: no separate cap for Xapian I/O (none added, per the ruling).

## Done (commits)

## Log

## Next

## Blocked on

Nothing.
