> Status: CURRENT (2026-11-02) the control plane, the caches and the stop as built through phase 7c step X6 and the task C3 wave; the design is `docs/design/phase7c.md` D1, D11, D13, D14, D16.

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
   that throws is logged, except the node setup and the shutdown checks, which end the process (`XapiandManager::control_task_failed`, status 70 or the `SystemExit` code, through the path a
   negative signal takes). The signal reader logs a signal that fails to run and reads the next. The housekeeping tick, an autocommit and an idle close log their failure and are tried again.
2. **`control::Periodic`** runs the minute housekeeping. Its wait is an event that the host's timer or `stop()` sets, so a stop does not wait out the minute. The registry's housekeeping pass is a
   task of its own that looks at 256 entries per turn under the pool's lock, spawns a task only for an entry that owes work, and yields between turns (`DatabasePool::housekeeping`).
3. **`control::Debounce`** is the autocommit. A write calls `XapiandManager::schedule_autocommit` (a lock on a small map, `touch`); the first touch of a key starts a watch
   task, a later one moves its target (`busy` after the touch, never past the first touch plus a random 8 to 10 s) and the key fires once it has been quiet for `debounce`. `stop()` ends the watchers without firing: closing an index commits what it has pending, so nothing is lost. After
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

## The stop: its order and its one bound

A signal reaches a task on the control loop, which runs `shutdown_checks`: the loop keeps running while requests are in flight (`http_clients`), and ends when they are done or after the old
`try_shutdown` retries (up to about 45 s measured). Then `join()` runs on the main thread, with one deadline, `XapiandManager::shutdown_write_bound` after it starts (35 s: the longer of a turn's deadline,
`DB_TIMEOUT` = 30 s, and the reactors' grace, 5 s, plus 5 s to write the responses). Every step below ends by it:

1. The housekeeping stops, the signal read is cancelled, and the control tasks drain (`drain_control`).
2. **Stop 1, the HTTP service**: `http::HttpAsioService::stop()` on a thread of its own (`shutdown_wait::wait`, `src/shutdown_wait.h`). It requests every session's and request's token, closes the acceptors, lets the sessions finish
   within the grace (5 s), closes the sockets stalled peers still hold, lets them finish within a second grace, then stops the loops: `stop()` returns within twice the grace. A read ends at its next
   checkpoint; an admitted write finishes within that, or is left suspended for the phases below.
   Then the tasks left suspended on the stopped loops (on a turn, a chore, a body read) go, in task's teardown phases (task `docs/design/cancellation.md` D8):
   `IndexChores::cancel_all()` drops every index's queued chores (their awaiters resume with `task::cancelled`) and waits for the running ones; `HttpAsioService::close()` closes
   every reactor's scheduler (`post` drops, `spawn` destroys its task); then `destroy_roots()` destroys the tasks still suspended on them, which releases their turns and shard uses before the indexes close. A task is counted by its
   frame, and the control plane's published wakes are cleared by a scope guard, so a destroyed task leaves no count and no dead event behind.
3. **Stop 2, the autocommits**: the scheduler stops; the waits that had not fired are dropped, because closing an index commits what it has pending. Then it drains.
4. **Stop 3, the indexes**: `DatabasePool::finish()` and `join()` close every index nobody uses (its turn, then its close chore on the `WorkPool`), polling in slices of at most 500 ms.
5. **Stop 4, the write-ahead logs** finish (closing an index appended to them, so they come after the indexes); **Stop 5, the fsync threads** finish.
6. The control loop stops, and its host's scheduler closes and destroys its roots. The hosts are destroyed before the service whose io_contexts they post to; then the
   registry (and with it the `WorkPool`), the logs and the schemas.

D16 states this order: the acceptors close at the start of `join()`, after the clients wait, and the loops stop before the indexes close (the registry closes on the main thread, so it needs none). The
order is logged (`Stop 1` to `Stop 5`) and tested (`test_node`: `test_the_stop_runs_in_its_stated_order`).

If the service has not stopped by the deadline, or an index is still in use at it, the stop logs what was unfinished (requests in flight per loop, and each index whose turn or references are held), keeps
the service alive for the thread that still waits in it, finishes the write-ahead logs and the fsync threads, and exits with status 75 (`EX_TEMPFAIL`) after flushing the log, without destroying what the abandoned
threads use. A write that was not acknowledged was never promised to be durable; an acknowledged one is in the log.

A second SIGINT or SIGTERM from the start of `join()` on (`XapiandManager::shutdown_signals`, counted by the signal handler) ends the wait and the registry's at once, with the same report and exit.
The worst case after the graceful wait for clients is the one bound, 35 s, and the flush of the logs, which has no limit of its own. The control tasks' and the autocommits' drains end by the same deadline.
`docs/src/content/docs/options.md` ("Stopping the node") tells operators.

## Limits

- The control loop's probe stays at its 100 ms idle interval unless something calls `note_activity()` on it; a task started with `Tasks::spawn` does not. Tests keep it at 1 ms by hand.
- A hung disk keeps a chore running and its index's turn held past the bound: the shutdown abandons that index and exits 75, and the next start replays its log.
