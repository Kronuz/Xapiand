> Status: CURRENT (2026-11-02) code review of Phase 7c step X5 (`phase7c/x5-control`, `Xapiand:17bcf2f1b` through `Xapiand:ab46afd05`, from master `Xapiand:d5a2e746c`). Verdict: APPROVE WITH CONDITIONS.

# Phase 7c X5 code review: the caches, the control plane as tasks

Reviewer: independent code reviewer, fresh context. Subject: the branch's diff against its design (`docs/design/phase7c.md` D1, D11, D13, D14, D16; `docs/reviews/phase7c-design.md` C10, C14), the ledger `docs/status/phase7c-x5.md` and the note `docs/mechanisms/control-plane.md`. Release build at `.scratch/build-rel` (the branch head, git `f337d1e05` in the binary's banner, which predates only the ledger commit and the K4 follow-up). The developer's proof was not redone (ctest, E2E, TSan, mutants). Probes are in `.scratch/review-x5/`.

## Verdict: APPROVE WITH CONDITIONS

One blocker, small and local: a failure while the node sets itself up (a taken port, any exception in `setup_node_task`) is now swallowed, and the process stays up serving nothing. Everything else probed held: `SingleFlight` is race-free and retries after a failure, 210,971 acknowledged writes survived SIGTERM under load with none missing, shutdown timing equals master's, and the control loop's minute tick is cheap at default sizes.

## Blockers

### X5-B1. A failure in node setup no longer ends the process (regression)

`XapiandManager::setup_node_task` (`src/manager.cc`, the task that runs `make_servers()` and `set_ready_impl()`) is spawned through `control::Tasks::spawn`, whose `counted()` (`src/control_plane.h`) catches every exception and hands it to `Tasks::failed`, which only logs `A control task failed: ...` (`manager.cc`, `Reactors::Reactors`). On master the same code ran in `control_offload`'s completion, so an exception was rethrown out of `run_loop()` and the process ended.

Probe `.scratch/review-x5/bind_conflict.sh BIN PORT TAG` (starts a node, then a second one on the same port):

```
x5 head : second node STILL ALIVE after 6 s (log: "Servers listening on TCP :18792", then "A control task failed: bind: Address already in use")
          curl :18792 answers from the FIRST node; the second one serves nothing and never exits (only SIGTERM ends it)
master  : second node exited after <1 s (log: "Exception: bind: Address already in use"; status 0 on master)
```

(Master was represented by the X4 branch's Release binary, `Xapiand-wt/x4/.scratch/build-rel/bin/xapiand`, which has the same base `Xapiand:d5a2e746c` and does not touch `manager.cc`'s setup.) A node that cannot bind, or fails any other step of setup, now looks started (it logs "Servers listening") and is dead weight under a supervisor, a test harness or a restart where the old process still held the port for a moment. Nothing retries.

Condition: a failure of `setup_node_task` (and of `shutdown_checks`, which can leave a shutdown half-done) must end the process the way master did, with a nonzero status if possible (`EX_CONFIG` or `EX_SOFTWARE`; master's status 0 was itself a defect, record it and do not preserve it). Smallest fix: catch in `setup_node_task`, log, and `shutdown_sig(-EX_SOFTWARE, ...)`/`break_loop()` with `atom_sig` set, the path `shutdown_sig(sig < 0)` already has. Test: `bind_conflict.sh` as a harness check, or `test_node` with a second `RunningNode` on a taken port, asserting the child exits within a few seconds. Mutant: swallow the exception again, the test must fail.

## Non-blocking notes

### X5-N1. The minute tick is a 5 to 15 ms stall at 10,000 open entries; slice it

Probe `pool_scan_probe.cc` (built from the `xapiand_test_pool` flags and objects; `pool_scan_10000.out` is the output): a `DatabasePool` with N indexes opened for real (one document each), `set_control` to an `AsioHost` loop, then `cleanup()` and `housekeeping()` posted to the loop as the `Periodic` tick does, with a 1 ms ticker on the same loop.

| open entries | `endpoints()` copy (holds the pool mutex) | `cleanup()` | `housekeeping()` on the loop | until its detached tasks drained | max 1 ms-ticker gap |
|---|---|---|---|---|---|
| 1,000 | 0.01 to 0.08 ms | 0.004 ms | 0.3 to 0.6 ms | 0.8 to 1.2 ms | 1.6 ms |
| 10,000 | 0.18 to 0.86 ms | 0.006 ms | 4.7 to 6.8 ms | 11.9 to 15.0 ms | 15.0 ms |

Linear, about 0.5 microseconds of synchronous work per open entry plus a coroutine and a post each. Against T4's 1 ms row for the control loop it is 5 to 15 times over, once a minute, at an entry count the default pool (`DATABASE_POOL_SIZE` 200) does not reach; at 100,000 it would be 50 to 150 ms. The loop serves timers, signals and autocommits only, so the effect is an autocommit or a signal read late by that much, once a minute: it does not touch p99 of requests. `cleanup()` is O(1) when nothing is idle (it stops at the first non-idle LRU entry). Recommendation: slice `housekeeping()` (and `endpoints()`, whose copy holds the pool mutex that every request thread takes) into chunks of a few hundred entries with a yield between, or skip the entries whose `merges_pending()` is zero without spawning a task for them (the task is spawned for every open entry, and for almost all of them returns at once: `ShardEndpoint::housekeeping`, `src/database/shard.cc:598`). Do it when the pool size option is raised past a few thousand, or in X6; not a reason to hold X5.

### X5-N2. Other synchronous work left on the control loop

Looked for in the loop's callbacks (`signal_sig_impl`, `shutdown_checks`, `setup_node_task`, `housekeeping_tick`, `Debounce::watch`):

1. No file read remains on a request path or in a steady-state tick. `schemas->cleanup()` is two `trim()`s under their mutexes (cost is the number evicted; not measured).
2. `SIGUSR1` runs `traceback::dump_callstacks()`, `dump_databases()` and `dump_schemas()` and prints them on the loop (`manager.cc:584`): O(entries) formatting and a write to stderr. Diagnostic only; note it for X6.
3. Startup (before the listeners open) reads TLS material, binds, and snapshots callstacks on the loop in `setup_node_task`; acceptable at startup, and it is why X5-B1 matters (a failure there is silent).

### X5-N3. `--committers` is dead and still reported

`--committers` (`src/opts.cc:166`) sizes nothing; `search_views.cc:756` still reports `num_committers` in the stats object, a number that describes no thread. Both go in X6 (D15), as the lead noted; add the stats key to that step's list. Also stale: `ARCHITECTURE.md:96` draws `DatabaseCleanup (std::thread, 60s)` and "committer / fsyncher (debouncers)" in the thread diagram. No code path, script or doc still expects a committer or cleanup thread: grep of `src tests harness CMakeLists.txt docs/dev docs/src` for `committer()`, `DatabaseCleanup`, `database_cleanup`, `control_offload`, `ThreadPolicyType::committers` finds none outside the lines above.

### X5-N4. The 35 s bound is not the whole shutdown; say what the operator can expect

After the bound, `join()` waits up to another bound for the registry (`manager.cc` pool join loop), and `drain_control` has a 60 s limit, twice. The changelog says "waits for unfinished writes for at most 35 s ... exits with status 75"; the mechanism note says it closes the registry "for at most another bound". The worst case is therefore about 70 s after the HTTP stop began, and the HTTP stop itself starts only after `ready_to_end_http` (graceful: waits for clients; measured at about 42 s on master and x5 alike with clients that keep sending). A supervisor's stop timeout should be set from that, and `docs/src` has no page that gives it (exit 75 is documented only in the changelog and the mechanism note). A second SIGINT or SIGTERM during the bound cannot shorten it: `shutdown_wait::wait` does not look at `atom_sig`, and the signal reader is cancelled by then. Master had the same property (an unbounded blocking `http_service->stop()`), so this is not a regression; making the wait check `atom_sig < 0` is a few lines.

### X5-N5. The abandoned path's durability claim has no restart test

"An acknowledged write is in the log" is the justification for abandoning writes (D16, C14), and it holds in everything I ran, but `test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound` (`tests/test_node.cc:527`) checks the exit status 75, the report lines and the time, and writes nothing before the shutdown that it then looks for after a restart. There is a short window: a stuck write that gets its turn during the extra registry wait can apply, enqueue its WAL record and answer 2xx; if that lands after `wal_writer->finish()` and before `_Exit(EX_TEMPFAIL)` (`manager.cc`, the `abandoned_http_` branch), the acknowledgement is not in the log. Probability is low (it needs the turn released in that last interval), and the WAL is asynchronous by default in any case, but the claim should be tested or the order changed (answer no write once `database_pool->finish()` ran). Add to the abandon test: a few acknowledged writes to another index before the shutdown, a restart of the data directory, and a read of them.

### X5-N6. SingleFlight edges (all probed with `sf_probe.cc` under ASan+UBSan and TSan)

Results first:

| probe | result |
|---|---|
| `fail`: leader throws, 4 joiners | all 5 got the error, 2 loads in all, the next call loaded again and succeeded (no cached failure) |
| `throwint`: leader throws a non-`std::exception` | rethrown to all 3 |
| `stress`: 8 threads, 160,000 calls, 3 keys, 1/3 of leaders throwing `runtime_error` or `cancelled` | every call returned its value or its own error class, 0 wrong values, `waiting()` back to 0; ASan+UBSan and TSan clean |
| `stress_wait`: 2,000 rounds of 4 callers on a leader held on an event, leader outcome rotating success, error, cancelled | 8,000 of 8,000 finished, `waiting()` 0, TSan clean |
| `hang`: leader never completes | the 3 joiners wait with no bound (expected: no timeout), all finish when released |
| `destroyjoiner`: a joiner's frame destroyed while suspended on the flight's event | **ASan use-after-poison in `task::event::set()`** when the leader completes |

Notes: (a) `destroyjoiner` is the `task::event` contract (the header says "Waiters must run to completion"), and no production path destroys a suspended joiner today (a client going away cancels at checkpoints, and an abandoned shutdown destroys nothing). It becomes real if a later step destroys request frames on shutdown or a deadline; record it beside `SingleFlight::run` as a precondition. (b) A hung leader hangs every first use of that key, the same as every request hanging on its own read before; joiners are not cancellable while waiting, but neither is a read inside a turn wait. The bound is the turn's `DB_TIMEOUT`. (c) The leader's error reaches every joiner, including a joiner whose own deadline was longer (a `busy` from the leader's turn wait); acceptable and arguably better than N separate 30 s waits.

### X5-N7. Schema-cache invalidation racing a flight: read, not a problem

By reading `schemas_lru.cc:200-330`: a flight can only be joined on a first use (nothing cached, `local_schema_ptr` null). A writer stores its new schema in the slot before it is acknowledged, so a request after the acknowledgement finds it cached and does not join; a leader or joiner that finishes with the old metadata finds the slot non-null and takes the cached schema (`failure = true` path), it does not overwrite. The `versions` outdated check applies only when something is cached, and so never to a flight. The settings resolver has the pre-existing load-then-store exposure (an invalidation between the read and the store leaves the stale value cached until it ages), unchanged in kind and not lengthened by joiners. Not probed beyond the cold-start storm below.

## What was probed and held

1. **SIGTERM during setup** (`sigterm_setup.sh`): at 0, 20, 50, 100, 200, 350, 500, 800 and 1200 ms after launch the node exits in 0.05 to 0.17 s with status 143, 70 or 0, never hangs, no assert; the x4 binary gives the identical table (statuses 143 and 70 come from `main.cc`'s handler before the manager exists, not from X5). The setup task has no suspension point, so a signal cannot interleave with it on the loop.
2. **SIGTERM during autocommit, and during load** (`ack_durability.py`, `term_during_load.py`): 6 writer threads over 4 indexes, SIGTERM 1 to 3 s in, 4 rounds, restart, read back every acknowledged id: 210,971 acknowledged, 0 missing, all exits 0. `term_during_load.py`: 8 threads on 4 existing indexes plus a fresh one per thread, SIGTERM 0 to 150 ms after the node is ready, so first uses are in flight: 8 rounds, 135 acknowledged, 0 missing, exit 0, shutdown 5.1 s each (the 5 s is the try-shutdown timer; x4 shows 5.06 to 5.09 s). With clients that keep sending, shutdown took 41 to 47 s on x5 and 42 to 46 s on the x4 binary: graceful-wait behaviour that predates X5.
3. **Cold-start storm** (`cold_storm.py`): fresh node, 32 threads, 40 new indexes, 1,920 mixed requests (PUT with fresh field names, GET, search): x5 p50 143 ms, p99 5.7 s, max 7.2 s, 0 errors other than my own wrong-verb 405s; x4 binary p50 216 ms, p99 5.4 s, max 7.3 s. No deadlock between a flight and an index turn; exit 0.
4. **Unit tests**: `xapiand_test_node` on Release, 48 checks, including the abandon test, pass.

## Not run

TSan and ASan of the node probes (only `SingleFlight` was run under sanitizers by me; the developer's TSan result for the node test stands); Debug build; the full E2E, mutants and ctest (the developer's, not redone); an abandoned-shutdown run with acknowledged writes and a restart (X5-N5); a second signal during the bound; the 60 s `drain_control` limit path; Linux and FreeBSD (CI); `schemas->cleanup()` at a large cache; the settings-resolver invalidation race.

## Probes kept

`.scratch/review-x5/`: `sf_probe.cc` (+ `sf_probe_asan`, `sf_probe_tsan`), `pool_scan_probe.cc` (+ `flags.txt`, `defs.txt`, `incs.txt`, `pool_scan_10000.out`), `sigterm_setup.sh`, `ack_durability.py`, `ack_master_proxy.out`, `term_during_load.py`, `cold_storm.py`, `bind_conflict.sh`. The data directories there (`scan_probe_data` 156 MB, `d_ack`, `d_ack_m`, `d_storm`, `d_storm_m`, `d_bind_*`) are scratch and can be deleted when disk is needed. No servers of mine are running.

## Re-check

Not yet. X5-B1 only; the rest are follow-ups (X6 for N3, N1 when the pool size grows).
