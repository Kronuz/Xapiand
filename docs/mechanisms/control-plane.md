> Status: CURRENT (2026-11-01) the control plane and the caches as built on `phase7c/x5-control` (phase 7c step X5); the design is `docs/design/phase7c.md` D1, D11, D13, D16. Not reviewed.

# The control plane and the caches

## What

The control loop runs the node's own work as tasks on its host: the setup, the shutdown checks, the signal reader, the minute housekeeping and the debounced autocommit. Nothing on it
waits on a thread. A task that has to wait awaits a timer, an event or a chore, and a `stop()` ends one that waits at once. The schema and settings caches take no lock across a read, and
concurrent first uses of one schema, or of one index's settings, read it once.

## How

```
 control loop (reactor::Reactor + AsioHost)
   signal_loop        co_await the self-pipe read              (cancelled by join)
   Periodic           timer 60 s -> cleanup(), housekeeping(), schemas->cleanup()
   Debounce<Endpoint> touch() from any thread -> one watch task per pending key -> committer_commit -> run_detached(autocommit)
   setup, shutdown checks   spawned by the loop's signals
```

1. **`control::Tasks`** (`src/control_plane.h`) spawns a task on a host and counts it until it ends; `join` runs the stopped loop on the main thread until the counts reach zero. A task
   that throws is logged, never fatal.
2. **`control::Periodic`** replaces the cleanup thread. Its wait is an event that the host's timer or `stop()` sets, so a stop does not wait out the minute.
3. **`control::Debounce`** replaces the committer threads. A write calls `XapiandManager::schedule_autocommit` (a lock on a small map, `touch`); the first touch of a key starts a watch
   task, a later one moves its target (`busy` after the touch, never past the first touch plus a random 8 to 10 s) and the key fires once it has been quiet for `debounce`. The throttle
   window of the old debouncer had no effect on timing and is gone. `stop()` ends the watchers without firing: closing an index commits what it has pending, so nothing is lost. After
   `stop()`, `touch` returns false and the writer commits now.
4. **`SingleFlight<Key, Value>`** (`src/single_flight.h`) is the caches' single flight. The first caller of `run(key, load)` loads; the others wait on a `task::event` and get the value, or
   the error. A leader that was cancelled or whose frame was destroyed leaves its waiters to load for themselves. The map's lock covers a lookup or an erase.
5. **Where a flight is used.** `SchemasLRU` joins one flight for an index's schema metadata and one for a foreign schema, only on a first use (nothing cached) and only outside a nested load
   (`context` empty): a nested load never waits for a flight, so a load cannot wait for itself. The resolver joins one flight per index for `load_settings`, unless the call is a reload. The
   maps' own locks stay short (O(log n) lookups, inserts and trims) and are never held across a read.
6. **Startup reads.** `preloadStopWords()` and `preload_mime_types()` read their files in `XapiandManager::init()`, before any listener opens; a lookup after that reads an immutable table.
   `kind_of` is gone with the Xapian engine, the census is compiled out of every gate build, and nothing in Xapiand, strata or prospect calls the spinning
   `published_position`, so inventory J has no read left on a request path.
7. **The node name** is read and saved by `run()` on the main thread, before the loop exists.

## Shutdown's bound on unfinished writes

`join` stops the HTTP service on a thread of its own and waits for it for `XapiandManager::shutdown_write_bound` (35 s: the longer of a turn's deadline, `DB_TIMEOUT` = 30 s, and the reactors'
grace, 5 s, plus 5 s to write the responses). Reads end at their next checkpoint, and sockets a stalled peer holds are closed after the grace; an admitted write finishes, or answers `503` when
its turn does not come in 30 s. If the service has not stopped at the bound, the shutdown logs what was unfinished (requests in flight per loop, and each index whose turn or references are
held), keeps the service alive for the thread that still waits in it, closes the registry for at most another bound, finishes the WAL writers and the fsynchers, and exits with status 75 (`EX_TEMPFAIL`) after flushing the log, without destroying what the abandoned threads use. A write
that was not acknowledged was never promised to be durable; an acknowledged one is in the log. `shutdown_wait::wait` (`src/shutdown_wait.h`) is the wait.

## Limits

- The control loop's probe stays at its 100 ms idle interval unless something calls `note_activity()` on it; a task started with `Tasks::spawn` does not. Tests keep it at 1 ms by hand.
- A hung disk keeps a chore running and its index's turn held past the bound: the shutdown abandons that index and exits 75, and the next start replays its log.
- `--committers` no longer sizes anything. D15 removes the option.
