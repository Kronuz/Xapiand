> Status: DRAFT (2026-11-08) the index registry as strata's catalog's payload, as built on `quest/xapiand/x-pool` (X-POOL steps 2 to 4) against strata main (strata:5d742b2, items 5 and 5b); the design is `docs/design/x-pool.md` revision 3 (D1 to D13). Not reviewed. Supersedes the open, close and housekeeping parts of `index-registry-and-lease.md`; the turn, the chores and the commit flight there still hold.

# The index registry on strata's catalog

## What

`DatabasePool` (`src/database/pool.{h,cc}`) is one `strata::catalog<prospect::index, ShardEndpoint>` keyed by an
index's path (`IndexCatalog`). The catalog opens, wakes, demotes, replaces and closes indexes; Xapiand's entry,
`ShardEndpoint`, is the catalog's payload `H`: built in place when the index opens and destroyed before the index
when it closes. A request names an index only through an `IndexPin`, a catalog pin.

## How

```
 lock_shard::pin() ─▶ pool.pin(path, flags, token) ─▶ catalog.pin_request(path, use, open | create, token)
   slot free/failed ─▶ open chore: resolve(<path>/strata) ─▶ open_index ─▶ open_payload: emplace ShardEndpoint,
                       borrow the index, open the blobs, replay the WAL ─▶ pins delivered
 autocommit, FLUSH ─▶ pool.pin_chore(path, token) ─▶ catalog.pin_chore(path, resident, token): never opens; empty when absent
 autocommit ─▶ commit ─▶ settle_merges: check in, sync a merge's output ─▶ pending or busy? arm it again
 catalog chores (bound close, shutdown, replace, a merge-only demotion) ─▶ host::commit ─▶ commit_blocking()
```

1. **Pins.** `pin(endpoint, flags)` asks `create` when the flags have `DB_CREATE_OR_OPEN`, else `open`. A missing
   index is the catalog's negative entry (`index_not_created`, a 404's `not_found`); only a create pin passes it.
   `pin_chore` is `resident`: no slot, a failed or closing one, and an open that fails all give an empty pin (C25).
   Pins are awaited before any view (`check_no_view_held`, D4). Once `finish()` ran, a request pin is served only
   for an index that is open or owed: a resident pin taken first answers whether it is, and holds it while the
   request pin is taken; a closed one is `not_available` (N3).
   Every pin waits with the request's token: `lock_shard` and `Shard::ready` pass theirs (the handler's budget's), and
   FLUSH its request's. A requested token ends the wait with `task::cancelled`; a free pin is taken whatever the
   token, and a requested one still starts the open or wake it asked for. The autocommit's pin has no token.
   A request takes one pin per key: `search_batch` pins each path once and searches its engine at every position
   that names it, since a task's second pin of a key waits for itself while the index is replaced.
2. **The open** (`open_payload`, D6). `resolve` refuses a missing `<path>/strata` unless creating and returns a
   `counted_io` over a `PosixIO`; `open_index` reads the pool's options at each open. `ShardEndpoint::open_blocking`
   takes the index's reader handle, makes the engine shard with `engine::strata::borrow` (a Core that points at the
   catalog's index; its reopen and close raise), opens the blob storage and replays the WAL with chores inline.
   The slot is `opening` throughout, so nothing sees the index before the replay ends; a throw fails the open.
3. **The turn.** `take_turn` only locks: a turn is taken under a pin, so the index is open. Its release checks the
   index in (`check_in()`) before it unlocks, which publishes finished merges and frees released views (D5).
4. **The commit chore** (`commit_blocking`, D7). With nothing applied since the last commit it writes no revision
   and no WAL line, and commits the index alone when a finished merge's output is all it has to sync, so a demotion
   can take (C25). Otherwise it takes the free turn, runs the engine on inline chores (it is itself a chore of the
   pool), and runs `do_commit`: blobs, engine, WAL COMMIT line.
5. **A failed use** marks its `Shard` stale (`retry_read`, `collect_failure`); `Shard::reopen()` hands the pin to
   `catalog.reopen()` and pins again by key. A write or commit that fails as only a fresh open fixes (a fence is
   `errc::database`; also `closed`, `reopen`, `corrupt`) marks the entry stale (`note_failure`), and every use of it
   hands its pin back when it ends (`Shard::release`). The catalog replaces the index once the other holders let go,
   and the new open replays the WAL (D2, F4).
6. **The autocommit** (`autocommit`, D8) commits under a resident pin, then settles the merges in a turn of its own
   (`settle_merges`): in a flush chore it checks the index in, which publishes the merges that finished, and commits
   the index when a published merge's output is all it has to sync (no revision, no WAL line). It returns true while
   a merge is still pending or its output unsynced, or when a turn did not come (`busy`); the committer
   (`autocommit_and_rearm`) then touches the debounce again. A busy index whose pins never all go, with no write to
   take its turn, gets its merges published and synced this way, and a write whose autocommit met a held turn is
   committed on a later one.
7. **FLUSH** pins every key of `catalog.snapshot()` resident and commits it, one key at a time (a task holds one
   turn); a key closed meanwhile was committed before its close, since an idle close takes only a committed index (D8). `busy()`, `count()` and `dump_databases()` read the snapshot, and `shutdown_report()` once the
   shutdown began.
8. **Shutdown** (D10, C24). `join()` starts `catalog.shutdown()` once and waits for it until its deadline; the
   awaiting coroutine is resumed inline by whoever releases the last pin. The manager's stop destroys every root that
   may hold a pin before it awaits `join()`: the HTTP loops' roots (stop 1), the autocommits drained (stop 2), then
   the control loop's suspended roots, and only then the indexes (stop 3). A root destroyed while it waits for a pin
   abandons its waiter, and the catalog releases the pin it delivers to it (strata's destroy-safe waiters), so no
   destroyed root keeps an index. `~DatabasePool` waits for the shutdown before it destroys the catalog; past
   `patience_` (30 s) debug builds abort naming what still holds an index.
9. **Tests** give the indexes' files another IO with `set_index_io` (strata's faultfs), so a sync can fail and fence.
10. **The timer** the catalog needs runs on the control loop's timers: each callback is an Asio timer armed on the
   loop's thread with `engine::Host::start_timer` (`DatabasePool::Timer`), so no thread of its own; a callback only
   posts a chore. The pool is given the loop at construction (the manager's `_rx` is made before the pool and
   destroyed after it; tests use `control_loop()` in `tests/pool_support.h`). Once the loop has stopped, the
   manager's stop runs it on its own thread while it waits (stops 1 to 3, a poll every 50 ms and 10 ms), and
   `~DatabasePool` runs what is still armed (the sweep's next pass) at once before it destroys the catalog, whose
   destructor waits for every armed timer.
11. **The sweep** closes idle indexes, the only idle close: it demotes an unpinned hot index idle for `demote_idle`,
    closes it idle for `close_idle`, and never touches a pinned one. `IndexSweep` (a `DatabasePool` argument) sets its
    period, those times and the negative entries' TTL; zero keeps strata's (1 s, 30 s, 10 min, 10 s), which a node
    uses. Its close syncs a finished merge's output through the entry's commit chore before the demotion (S14). A
    pass erases a missing index's negative entry once it is `negative_ttl` old.
12. **Owed answers** (D9). Under `defer_io` a write's apply may answer `flush_owed`, `drain_owed` or `map_owed` with
    nothing applied; `apply_settling` (`src/engine/strata/owed.h`) runs `refresh()`, `reclaim_for_space()` or
    `promote()` in a flush chore of the turn and applies the same transaction again, at most twice; a third owed
    answer is `errc::internal`. `map_owed` comes to a write under a resident pin on an `owed` (demoted) index, which
    the catalog grants without a wake; a request pin wakes the index first, so its writes never meet it.
13. **The budget** (D11). `DatabasePool::soft_open()` is the pool size, at most the budget's descriptor soft cap,
    and the catalog's `soft_open`; a search's batch (`search_batch_indexes()`, K) is at most it.
14. **The lease assertion** (D12). Debug builds assert that the index's `unleased_maps()` is zero after each turn's
    check-in (`Turn::release`) and at the end of each use (`Shard::release`): every map is made under the lease.

## Why

- One catalog owns every open, close and replacement, so the pool's own state machine, single flight, idle close,
  housekeeping pass, LRU and reference counts go; there is one way to hold an index from Xapiand's registry.
- Strata's index-owned reader registry (R4.1) caps the views of one index; `StrataReaders` and its admission gate
  were a second registry in front of it (XH2).

## Limits

- The hostless path (`engine::open_local`, `engine::check`; only the engine's tests call it) keeps the adapter's own
  registry of owned indexes and its in-place reopen (D2 as ruled, CR6); the index's directory lock keeps it and the
  catalog from holding one index at once.
- A task that holds two pins on one key and hands one back waits for itself: the replace drains the other pin.
  `search_batch` takes one pin per key; any other caller must too. Only a request's token or deadline ends that wait.
- A cancelled create request may still create its index: the open it started goes on.
- `corrupt_segment` replaces the index (a reopen), not `heal()` under the turn (D2).
- The restore's open replays a WAL that exists; the old open skipped the replay when the opener's flags disabled the WAL.
- While a merge runs, the autocommit comes back at the debounce's period (a turn and a flush chore each time) until
  the merge is published and synced; an index whose merges never end keeps it coming.
- An idle close (the sweep's) takes only a committed index; an uncommitted one waits for its autocommit, the bound
  close, or the shutdown, which commit through the entry.
- The shutdown's report names what holds an index as the catalog sees it (`pinned`, `a view is held`); a held turn
  shows as `pinned`, since a turn is held only under a pin.

## Code

`src/database/pool.{h,cc}` (`DatabasePool`, `IndexPin`, `ShardEndpoint` pin side, `Turn`), `src/database/shard.cc`
(`open_blocking`, `commit_blocking`, `settle_merges`, `autocommit`, `Shard` and its reopen), `src/database/handler.cc`
(`autocommit_and_rearm`), `src/database/lock.h`, `src/engine/strata/shard.cc`
(`borrow`, the Core's `borrowed` and `owned`, `apply_t`), `src/engine/strata/owed.h` (`apply_settling`). Tests: `tests/test_pool.cc` (`test_a_create_pin_passes_a_missing_index`,
`test_a_pin_keeps_its_index_open`, `test_a_chore_pin_is_resident`, `test_flush_commits_every_index`,
`test_the_shutdown_commits_through_the_entry`, `test_a_resident_commit_on_an_owed_index_lets_it_close`,
`test_join_waits_for_every_pin`, `test_a_failed_use_hands_its_pin_back`, `test_a_finishing_pool_serves_an_open_index`,
`test_a_cold_pin_reads_what_the_log_replays`, `test_a_fenced_index_is_replaced_and_replays_its_log`,
`test_the_commit_chore_logs_nothing_when_nothing_was_applied`, `test_the_pool_waits_for_a_pin_released_during_its_end`,
`test_the_autocommit_comes_back_while_a_merge_is_pending`, `test_a_busy_autocommit_comes_back`,
`test_the_sweep_closes_only_an_unpinned_index`, `test_the_shutdown_commits_and_logs_an_uncommitted_write`,
`test_the_sweeps_close_syncs_a_merges_output`, `test_a_root_destroyed_while_it_waits_for_a_pin_releases_it`,
`test_a_destroyed_root_that_held_a_pin_lets_the_pool_join`, `test_a_cancelled_pin_stops_waiting`,
`test_a_write_to_a_demoted_index_applies_after_one_promote`, `test_a_third_owed_answer_is_internal`,
`test_soft_open_is_at_most_the_descriptor_cap`, `test_the_sweep_erases_a_missing_index_negative_entries`,
`test_the_hostless_path_and_the_catalog_never_share_an_index`); `tests/test_node.cc`
(`test_a_busy_autocommit_comes_back_for_its_write`, `test_a_search_batch_pins_a_repeated_index_once`,
`test_a_cancelled_request_stops_waiting_for_its_index`, `test_a_cancelled_search_stops_waiting_for_its_index`).
