> Status: DRAFT (2026-11-03) the index registry as strata's catalog's payload, as built on `phase7c/x-pool` (X-POOL step 2, the H and pins slice) against strata `phase/cold-tier-step2-item5b`; the design is `docs/design/x-pool.md` revision 2 (D1 to D8, D10). Not reviewed. Supersedes the open, close and housekeeping parts of `index-registry-and-lease.md`; the turn, the chores and the commit flight there still hold.

# The index registry on strata's catalog

## What

`DatabasePool` (`src/database/pool.{h,cc}`) is one `strata::catalog<prospect::index, ShardEndpoint>` keyed by an
index's path (`IndexCatalog`). The catalog opens, wakes, demotes, replaces and closes indexes; Xapiand's entry,
`ShardEndpoint`, is the catalog's payload `H`: built in place when the index opens and destroyed before the index
when it closes. A request names an index only through an `IndexPin`, a catalog pin.

## How

```
 lock_shard::pin() ─▶ pool.pin(path, flags) ─▶ catalog.pin_request(path, use, open | create)
   slot free/failed ─▶ open chore: resolve(<path>/strata) ─▶ open_index ─▶ open_payload: emplace ShardEndpoint,
                       borrow the index, open the blobs, replay the WAL ─▶ pins delivered
 autocommit, FLUSH ─▶ pool.pin_chore(path) ─▶ catalog.pin_chore(path, resident): never opens; empty when absent
 catalog chores (bound close, shutdown, replace, a merge-only demotion) ─▶ host::commit ─▶ commit_blocking()
```

1. **Pins.** `pin(endpoint, flags)` asks `create` when the flags have `DB_CREATE_OR_OPEN`, else `open`. A missing
   index is the catalog's negative entry (`index_not_created`, a 404's `not_found`); only a create pin passes it.
   `pin_chore` is `resident`: no slot, a failed or closing one, and an open that fails all give an empty pin (C25).
   Pins are awaited before any view (`check_no_view_held`, D4). Once `finish()` ran, a request pin is served only
   for an index that is open or owed: a resident pin taken first answers whether it is, and holds it while the
   request pin is taken; a closed one is `not_available` (N3).
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
6. **FLUSH** pins every key of `catalog.snapshot()` resident and commits it, one key at a time (a task holds one
   turn); a key closed meanwhile was committed before its close, since an idle close takes only a committed index (D8). `busy()`, `count()` and `dump_databases()` read the snapshot, and `shutdown_report()` once the
   shutdown began.
7. **Shutdown** (D10, C24). `join()` starts `catalog.shutdown()` once and waits for it until its deadline; the
   awaiting coroutine is resumed inline by whoever releases the last pin. The manager's stop destroys every root that
   may hold a pin before it awaits `join()`: the HTTP loops' roots (stop 1), the autocommits drained (stop 2), then
   the control loop's suspended roots, and only then the indexes (stop 3). `~DatabasePool` waits for the shutdown before it destroys
   the catalog; past `patience_` (30 s) debug builds abort naming what still holds an index.
8. **Tests** give the indexes' files another IO with `set_index_io` (strata's faultfs), so a sync can fail and fence.
9. **The timer** the catalog needs is one `Scheduler` thread (the scheduler library's wheel); its callbacks only
   post. The catalog is destroyed before it, while its timers can still fire.

## Why

- One catalog owns every open, close and replacement, so the pool's own state machine, single flight, idle close,
  housekeeping pass, LRU and reference counts go; there is one way to hold an index from Xapiand's registry.
- Strata's index-owned reader registry (R4.1) caps the views of one index; `StrataReaders` and its admission gate
  were a second registry in front of it (XH2).

## Limits

- The hostless path (`engine::open_local`, tests and the CLI) still keeps the adapter's own registry of owned indexes
  and its in-place reopen; D2's private catalog for it is not built.
- A task that holds two pins on one key and hands one back waits for itself: the replace drains the other pin.
- `corrupt_segment` replaces the index (a reopen), not `heal()` under the turn (D2).
- The restore's open replays a WAL that exists; the old open skipped the replay when the opener's flags disabled the WAL.
- `map_owed` (D9), the budget's caps (D11) and the `unleased_maps()` assertion (D12) are step 4.
- An idle close (the sweep's) takes only a committed index; an uncommitted one waits for its autocommit, the bound
  close, or the shutdown, which commit through the entry.
- The shutdown's report names what holds an index as the catalog sees it (`pinned`, `a view is held`); a held turn
  shows as `pinned`, since a turn is held only under a pin.

## Code

`src/database/pool.{h,cc}` (`DatabasePool`, `IndexPin`, `ShardEndpoint` pin side, `Turn`), `src/database/shard.cc`
(`open_blocking`, `commit_blocking`, `Shard` and its reopen), `src/database/lock.h`, `src/engine/strata/shard.cc`
(`borrow`, the Core's `borrowed` and `owned`). Tests: `tests/test_pool.cc` (`test_a_create_pin_passes_a_missing_index`,
`test_a_pin_keeps_its_index_open`, `test_a_chore_pin_is_resident`, `test_flush_commits_every_index`,
`test_the_shutdown_commits_through_the_entry`, `test_a_resident_commit_on_an_owed_index_lets_it_close`,
`test_join_waits_for_every_pin`, `test_a_failed_use_hands_its_pin_back`, `test_a_finishing_pool_serves_an_open_index`,
`test_a_cold_pin_reads_what_the_log_replays`, `test_a_fenced_index_is_replaced_and_replays_its_log`,
`test_the_commit_chore_logs_nothing_when_nothing_was_applied`, `test_the_pool_waits_for_a_pin_released_during_its_end`).
