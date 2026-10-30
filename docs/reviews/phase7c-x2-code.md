> Status: CURRENT (2026-10-30) code review of Phase 7c step X2 (`phase7c/x2-registry-lease`, `Xapiand:f8657f445..7951486a9`, from master `Xapiand:e265acee6`). Reviewer: code-reviewer, fresh context.

# Code review: 7c X2 (registry, lease, host mode, chores)

## Verdict: APPROVE WITH CONDITIONS

The turn protocol, the close and idle-close state machine, commit coalescing and the shutdown drain hold up under reading and under the storms I
added. One blocker: tasks that run on the control loop (autocommit, idle close, housekeeping) take the engine's `core.mtx`, so the loop can stall
behind a commit chore or a long search. It is a small fix. Land after it; no second review needed beyond a re-check of B1.

## Blocking

**X2-B1. The control loop blocks on `core.mtx`.** Detached tasks (`DatabasePool::run_detached`, `pool.cc:451`) start on `AsioHost`, the manager's
control reactor (`manager.cc` `Reactors::host`), and run on that thread up to their first suspension and after every chore resume. Three of them call
engine methods that lock `core.mtx` (`strata/shard.cc` `Shard::merges_pending` at 1239, `Shard::revision` at ~1280):

1. `ShardEndpoint::housekeeping`, `shard.cc:581` (`eng->merges_pending()`), for every open index, once a minute.
2. `do_commit`, `shard.cc:680` and `:683` (`eng.revision()` before and after the commit chore), reached from the autocommit task and the idle close
   (`close_marked`) on the loop.

`core.mtx` is held across the whole commit/refresh/flush chore (`strata/shard.cc` `Shard::commit`, `reclaim`, `apply_t`'s chore) and, until X2b, across
a search. So the loop thread (which also runs the SIGTERM signal, `try_shutdown_timer` and every other index's autocommit) waits out an fsync or a
search. `engine::check_may_block` cannot see a mutex, which is why `async_check` and the 641/641 E2E pass.
Probe `.scratch/review-x2/probe_mtx.cc` (30,000 documents buffered, then a commit): `merges_pending()` takes 0.014 ms idle and **169.7 ms during the
commit, the whole commit (170.0 ms)**. A convoy of N open indexes each mid-commit makes it N such stalls in series. AGENTS.md: "Never block a reactor
thread."
Fix, any of: (a) read `merges_pending` and the revision from atomics the engine mirrors outside `core.mtx` (the revision is already `set_revision`'d in
`ShardEndpoint`; use `get_revision()` in `do_commit` for `prior_revision`, and take `current_revision` inside the commit chore); (b) run
`housekeeping`'s check inside the merge chore (`reclaim` already re-checks under the lock); (c) give the detached tasks a host that may block (a
`BlockingScheduler` thread) until X2b removes `core.mtx`. (a)+(b) keep the design (D7: timer tasks on the control loop). A regression test: hold
`core.mtx` from a thread (a long `scan`) and assert that `housekeeping()` and an autocommit return their first slice at once.

## Non-blocking notes

**X2-N1. `ShardEndpoint::uuid_` is written by the open chore and read without a lock.** `shard.cc:471` writes it on every open (a stale replacement
reopens); the data-storage header checks read it (`shard.cc:276`, `:301`) from the blob read chore (`storage_read_blocking`, `shard.cc:1117`), which
runs under no turn but a lock_shard ref, so it can overlap a stale-close-and-reopen. A `std::string` race (benign while the uuid is unchanged, UB
nonetheless; TSan would report it). Set it under `mtx` or keep it in a `shared_ptr<const std::string>`.

**X2-N2. A write with `commit=true` is two turns.** `Shard::add_document` (and its siblings, `shard.cc:1340` ff.) applies under one turn, releases, then
`commit()` takes the next. If that second turn times out the request answers 503 `busy` for a write that is applied, WAL-logged and
(autocommit is not scheduled on this path) committed only by the next commit or the idle close; a client retry of an auto-id add creates a second
document. Needs the architect: either one turn for write+commit (the commit then cannot coalesce) or a documented "busy after apply" answer that
does not tell the client to retry.

**X2-N3. `ensure_open` refuses an open entry once the node is finishing.** `pool.cc:~190` checks `is_finished()` before the state switch, so after
`DatabasePool::finish()` an autocommit, a commit request or a read on an already-open index throws `not_available`, and `housekeeping` logs a
warning per index per tick until the pool clears. Durability is intact (`clear()` -> `close_marked(true)` commits `modified`), the cost is errors for
requests in flight at shutdown and noise. Check `finished` only on the `empty` branch if "new opens only" is the intent.

**X2-N4. An autocommit that races an idle close can reopen the index it just lost.** `commit()` tests `modified` before `take_turn` (`shard.cc:607`);
during the close `modified` is still true, so the commit proceeds, `ensure_open` awaits the close, then opens the index again (open chore, WAL
replay) only to find `modified` false and return. The window is the close's duration, and the debounce (about 1 s) normally fires long before an
idle close (60 s overflow, 1 h otherwise), so churn only. Static reading; not probed.

**X2-N5. `test_a_commit_not_covered_does_not_share_the_outcome` exercises a state production cannot reach.** The join predicate `covers >= mine`
(`shard.cc:615`) is always true when a flight exists, because writes need the turn the flight's leader holds and a flight is published under it;
the test reaches the other branch with `PoolTestAccess::write_lands`. Mutant M9 is killed by that hook, not by an interleaving. Keep the predicate
(it is the right defensive check) but do not read the mutant kill as proof of a production race.

**X2-N6. Pin skew.** Xapiand pins strata `c82ac9f` ahead of prospect, whose main pins `16e9054` (ledger K1). One copy of strata is built, so there
is no ODR hazard, but prospect main's own CI has not seen `c82ac9f`. Record it on the lead's pin-follow-up list (prospect re-pin) before the next
library release.

**X2-N7. ODR lint.** I scanned every header under `src/` (not `xapian/`) that tests a `config.h` macro (`XAPIAND_*`, `HAVE_*`, `UUID_*`,
`USE_PROC_FOR_UUID`, `STRERROR_R_CHAR_P`, `SOCKLEN_T`) for a direct `#include "config.h"`. Only `src/tls.h` lacks one, and `XAPIAND_TLS` is a PUBLIC
compile definition of `XAPIAND_OBJ` (`CMakeLists.txt:1259`), so it is the same in every TU. The new X2 headers (`engine/host.h`, `chores.h`,
`open.h`, `pool.h`) carry no build-dependent layout (`turns_` in `BlockingScheduler` is unconditional). Suggest adding that grep to `preflight.sh`;
nothing else to fix.

**X2-N8. `ChoreAwaiter::invoke` with no scheduler resumes the coroutine on the pool worker** (`pool.cc:~95`, `h.resume()` when `task::current()` was
null at suspension). No production path does (every task runs under `BlockingScheduler` or `AsioHost`), but a coroutine that gets there continues on a
worker with no `Host`. An `assert(s)` states the contract.

## What I checked, by risk

1. **Turn protocol.** Every engine mutation goes through `Shard::*` -> `take_turn` -> `do_*`, `commit()`, `close_marked` (takes `turn.lock()` itself),
   `housekeeping`, `fresh()` (refresh under turn) or the open chore. Open runs without the turn only in state `opening`, which is reachable only from
   `empty`, which `close_marked` sets after it has finished with the engine. No turn holder calls `ensure_open`/`take_turn` again (the retry loop
   releases `mine` before looping), so the stale-close path cannot self-deadlock. A WAL line is enqueued inside the same turn as its apply
   (`do_add` ff.); the WAL writer keeps per-path order. The owed chore of `apply_t` runs inside the writer's turn, so no later position can overtake an
   owed transaction; a chore failure surfaces as `errc::database` -> stale -> replace, and `errc::internal` after four rounds fails the write.
   `.scratch/review-x2/probe_storm` (4 writers, a committer, an idle closer on a 1 ms loop, a stale marker every 1 or 7 ms, read-your-write after each
   add, 8 to 15 s): 0 write or read errors, 0 duplicate ids, 0 missing or wrong documents after reopen (1,091 and 828 acknowledged adds; 216 and 358 opens).
2. **Close races.** `begin_idle_close` and every `spawn`/`get`/`endpoints()` take the pool mutex, so `refs` is exact at the mark; a user that follows
   awaits `closed_`, which is set after `state_ = empty` and always set (the body catches everything). A waiter that gets the turn of a closed or
   replaced entry gives it back. `.scratch/review-x2/probe_wal_stale` (WAL on, five uncommitted adds, then a stale mark or an idle close, then a
   write): documents 1 to 5 present once, the new one gets id 6, no WAL quarantine, in both variants. Shutdown: `detached_` is counted before the
   control is read, the drain runs before `http_service->stop()` (later tasks run inline), and `clear()` commits `modified`. The hazard the fix
   closed has no sibling I could find except X2-N3 (noise) and X2-B1 (a stalled loop delays the drain, bounded by the stall).
3. **Commit coalescing.** A flight is published under the turn with `covers = applied_`; a joiner needs `covers >= applied_ at call`; a failure is
   `exception_ptr` shared by every joiner; the flight is cleared before `done` is set. See X2-N5 for the test's reach.
4. **Self-waits and blocking.** Xapiand makes no `WorkGroup::wait` call; chores that call `engine::sync_wait` (open, replay) run it with inline
   chores. `~WorkGroup` runs on the cleanup/main threads only. I rebuilt `xapiand` at HEAD (the developer's release binary predates the last commit)
   and ran the docs E2E: **641/641**, and the node log has no "blocks on database work" line
   (`.scratch/review-x2/e2e_blockguard.sh`, release logs each site once). The remaining reactor-path blocking I found is X2-B1.
5. **ODR.** X2-N7.

## Probes kept (all in `.scratch/review-x2/`)

- `probe_storm.cc` (usage `probe_storm SECS STALE_MS`), `probe_wal_stale.cc` (`stale` or `idle`), `probe_misc.cc` (commit and read on a missing index:
  commit returns false as master did, `get_view` throws `not_found`), `probe_mtx.cc` (X2-B1).
- `build_probe.sh SRC OUT` compiles a probe with `test_pool`'s flags and links it against the release build; `e2e_blockguard.sh` and `h/` (a copy of the
  E2E harness using 127.0.0.1 and the newman 6 install) run the docs E2E and grep the node log; run output in `run/` and `e2e/`.

## Not run

TSan, ASan/UBSan and Debug (asserts on) builds, mutants M1 to M9, the unit suites, `engine_crash_check.sh`, `graceful_shutdown_check.sh`,
`engine_mixed_load.py` (the developer's evidence stands; the host was loaded and my probes ran on the release build only); the control-loop stall was
measured through `Shard::merges_pending()` directly, not through a live `AsioHost`; X2-N4 and X2-N2 are by reading.
