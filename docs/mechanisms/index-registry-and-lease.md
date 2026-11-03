> Status: CURRENT (2026-10-30) the registry, the lease and host mode as built on `phase7c/x2-registry-lease` (phase 7c X2); the design is `docs/design/phase7c.md` D4 to D7 and D9. Not reviewed.

# The index registry and its lease

## What

`DatabasePool` (`src/database/pool.{h,cc}`) is a map from an index's path to a `ShardEndpoint`, its registry
entry. Opening an index never blocks a thread, and every operation on an index (a write, a refresh, a commit, a
close, the timers' commit and idle close) takes the entry's **turn**, one `task::async_mutex` per index. The turn
has a deadline: a request that does not get it answers `engine::errc::busy`, a 503. The engine runs in strata's
**host mode**: a write does no I/O of its own, and all device work (open, refresh, commit, close, blob I/O) is a
**chore** on the index's `WorkGroup`.

## How

```
 task (a request, a timer)            registry entry                        WorkPool (one per host)
 ───────────────────────────────      ────────────────────────────────      ───────────────────────
 ensure_open ───────────────────▶ state: empty ─▶ opening ─▶ open ─▶ closing ─▶ empty
   leader: open chore ─────────────────────────────────────────────────────▶ open, replay WAL
   others: await the flight's event (no thread waits)
 take_turn ─ lock_until(deadline) ▶ async_mutex (FIFO, one holder)
   holder: apply ─ owed? ── chore (refresh | reclaim_for_space) ─────────────▶ index's WorkGroup
           revision, WAL enqueue            (same transaction applied again)
 release
```

1. **State machine** (`ShardEndpoint::State`, under the entry's mutex, O(1) sections). `empty` → the first user
   becomes the leader and sets `opening`; the others take the leader's `OpenFlight` and await its event, and see
   its error if it failed. `open` → `closing` when the idle task finds no user (`refs == 0`: no `IndexPin`, `docs/design/x-pool.md` D3) under the pool's lock,
   or when a failure marked the engine stale; users that find `closing` await the entry's `closed` event, then
   go round again (and open a new engine). A task that gets the turn of an entry that closed or went stale while it
   was queued gives the turn back and starts over.
2. **The turn.** `take_turn` awaits the open, then `lock_until(host, deadline)` (a timer that `Host` provides),
   then rechecks the state. A `Turn` also counts, per host, the turns the running task holds, and debug builds
   assert it is at most one: schema writes finish, and release, before the document's turn starts.
3. **One unit.** A write's engine apply, its counters and its WAL enqueue (`ShardEndpoint::do_add` and the other
   `do_*`) happen inside one turn, so the WAL's order is the order the engine assigned ids in.
4. **Host mode** (`engine::strata::Shard` opened with `OpenOptions::host`, strata's `defer_io`). `apply` returns
   `flush_owed` or `drain_owed` instead of writing a segment or draining files. `apply_t` runs `refresh()` or
   `reclaim_for_space()` as a flush chore and applies the same transaction again, before the turn lets another
   write in. A read sees what was applied only after `refresh()`; `ShardEndpoint::fresh` awaits one (a turn and a
   query chore) when `dirty()`, and nothing otherwise.
5. **Commits** (`ShardEndpoint::commit`). A commit records the writes applied so far, takes the turn, runs the blob
   storage's fsync, the engine's commit and the WAL line as chores inside it, and publishes a `CommitFlight` with the
   position it covers. A commit request whose writes are at or below that position joins the flight and gets its
   outcome (a failure too); any other waits for the turn and commits itself.
6. **Chores** (`IndexChores`). A chore is a `WorkItem` in the entry's `WorkGroup`; it runs `fn` on a worker and
   resumes the awaiting task through the scheduler it suspended on, so the resume is a fresh slice on its own thread.
   `open` and `read of a blob` are `query` class, a commit, a close and a blob write `flush`, the idle
   `housekeeping` (publish finished merges, free queued files) `merge`. No chore waits on its own group: the
   WAL replay runs in the open chore with the engine's chores inline, then hands the engine to the group's.
7. **Hosts.** A task runs on a `Host`. `engine::sync_wait` runs it on a `BlockingScheduler` that belongs to the
   waiting thread: every resume (a chore finished, a turn handed over, an event set) is posted to that thread's
   queue, so nothing runs inside another task's stack, and a turn's deadline is a timer in that queue. The
   control loop's `AsioHost` runs the timers' tasks (the debounced autocommit, the idle close and housekeeping).

## Why

- A cv-and-checkout pool made every request thread wait for the device or for another request; the registry makes
  the wait a suspended task, bounded by a deadline, and moves the device work to a pool whose groups take fair
  turns.
- The turn is the only mutual exclusion on an index's writes, so the engine, the WAL and the revision cannot
  disagree about order. The idle close takes it too, so strata's single-writer lease is never entered by two
  threads (`owner_break` stays unreachable).
- Host mode keeps `apply` from doing I/O under the turn, so a request that holds the turn holds it for CPU only
  except while it awaits a chore it owes.

## Limits

- The handler, the schema and resolver code and the views are tasks (X2b), and a request awaits its view on its reactor (X3,
  `docs/mechanisms/requests-on-the-reactor.md`). A Lua script's foreign read still waits for the handler's tasks on its reactor
  (X4); `async_check.sh` lists it as owed.
- Reads take no turn: they hold a view of the index (`docs/mechanisms/reader-views.md`).
- A commit holds the turn through its fsyncs: writes to that index wait for them. A close holds it for the commit and the retirement
  only: the wait for readers that hold views happens after the turn is released (`ShardEndpoint::close_marked`).
- The turn deadline is a constant (`DB_TIMEOUT`, 30 s), not the request's remaining time.

## Code

`src/database/pool.{h,cc}` (`DatabasePool`, `ShardEndpoint`, `Turn`, `IndexChores`, `IndexPin`), `src/database/lock.h` (`lock_shard`), `src/database/shard.{h,cc}` (`Shard`,
the `do_*` writes, commit, open and close, the data storage in chores), `src/engine/host.h`, `src/engine/host_asio.h`,
`src/engine/chores.h`, `src/engine/strata/shard.cc` (`apply_t`, `refresh`, `close`).
Tests: `tests/test_pool.cc`, `tests/test_engine_host.cc`; `harness/engine_crash_check.sh --reactors 2`.
