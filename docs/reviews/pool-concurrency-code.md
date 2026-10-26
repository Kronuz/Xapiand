# Code review: fix/pool-concurrency (concurrency tier)

Branch `fix/pool-concurrency` at `8c4162849` against local master `c625977e7`. Reviewer: code reviewer, fresh context. Evidence is local builds only (no CI): ASan+UBSan (`build-asan`, AppleClang, relinked at review time, `xapiand_test_pool` 12 checks pass) and the contributor's TSan build (`build-tsan/bin/xapiand_test_pool`, 12 checks, 0 warnings, run once).

## Verdict: APPROVE WITH CONDITIONS

The three named fixes are sound: `cleanup_mtx` closes the LRU use-after-free, the checkin/unlock wake-ups are now ordered with the waiters' predicate, and `Shard::reopen` breaks after one open. The `clear()` rework holds up under adversarial reading and probes except at one place, and that place is in the checkin edit, not in `clear()`: the writable drop path of `ShardEndpoint::checkin` still lets a concurrent cleanup evict the endpoint between two critical sections, and the branch's new `lock_guard` on the endpoint mutex turns that old silent window into `std::terminate`. One required fix (CR1, three lines), one cheap test to add (CR2), the rest follow-ups.

## Blocking (condition to land)

1. CR1: writable-drop path of `checkin` takes `mtx` twice; the endpoint can be evicted between them and the second lock aborts the process.

## Conditions

1. Fix CR1 (single critical section) and keep `probe_drop_stress` as a unit test (it fails on the branch 3 of 3 within 10 s, passes with the fix).
2. Add the shared-shard `clear()` test from CR2 (deterministic, kills mutant MB). A rechecker confirms both.

## Findings

### CR1. Writable drop in `checkin` leaves a window where the endpoint is evictable, then locks a destroyed mutex (blocking)

`src/database/pool.cc:399-402` (and `:405-410`, the failed-autocommit variant) takes `mtx`, does `writable.reset()`, releases it. `:423-425` takes `mtx` again for `_busy.store(false)` and `writable_cond.notify_one()`. Between the two, `writable` is null and `readables` may be empty, so `ShardEndpoint::is_used()` (`:605-617`) is false. `checkin` holds no `ReferencedShardEndpoint` (`DatabasePool::checkin`, `pool.cc:998-1004`, calls `shard->endpoint.checkin(shard)` directly), so a concurrent `cleanup` evicts and frees the endpoint. The second section then locks and signals freed memory.

The ledger's claim "notifying under mtx also keeps the endpoint alive until checkin is done with it" is true only when the slot still holds the shard (the keep path) and for the readable branch (one critical section, `:429-440`). It is false for the writable drop branches, which run when the shard is closed, the endpoint is finished, or the endpoint is locked for an exclusive lock.

Why it matters more than on master: master notified without taking `mtx` (a silent notify on freed memory). The branch locks `mtx`; on macOS the lock on the destroyed mutex fails with `EINVAL`, libc++ throws `system_error`, and `checkin` is `noexcept`, so the process aborts. ASan does not report it (the access happens inside libc++'s uninstrumented `mutex::lock`), so the existing ASan tests cannot see it.

Evidence:

1. Deterministic: `probe_drop` under `drop_window.lldb`. Stops `checkin` at `pool.cc:423` (between the two sections), runs `database_pool.cleanup(true,false)` from the breakpoint, resumes: `libc++abi: terminating due to uncaught exception of type std::__1::system_error: mutex lock failed: Invalid argument`.
2. Race: `probe_drop_stress 10` (2 threads checking out, closing and checking in a writable on their own endpoints, 3 threads in `cleanup(true,false)`): the same abort, 3 of 3 runs, each within 10 s.
3. Fix verified: `pool_fixed.cc` (a copy of `pool.cc` with `drop` computed first, then one `lock_guard` section doing `writable.reset()` if dropping, `_busy.store(false)`, `notify_one()`): `probe_drop_stress_fixed 10` survives 3 of 3 runs (298402, 209342 and 185752 checkins). The WAL `delete_wal` call stays before the lock in that version.

`test_concurrent_cleanup` does not reach this path: its shards are never closed, so checkin always takes the keep path. Fix: restructure as `pool_fixed.cc` (compute `drop`, including the post-`autocommit` `is_closed()` check, then one section). No design question.

### CR2. `clear()` wake-ups and the shared-shard rule are covered by the hammer only; one is cheap to pin (condition for the MB test; the rest non-blocking)

Mutants I built (compiled `pool.cc` copies linked into `xapiand_test_pool`, ASan flags; `runmut.sh`, `pool_M*.cc`, `run_M*.log`):

| Mutant | Change | `test_pool` |
|---|---|---|
| MA (= contributor's M8) | clear does not `writable_cond.notify_all()` | passes (12 checks) |
| MB | clear drops the writable even when `use_count() > 1` | passes |
| MD | clear's readable keep path does not notify | passes |
| ME | clear's readable drop path does not notify | passes |
| MF | checkin readable does not `notify_one()` | FAILS (control: the harness works) |

1. MB is catchable deterministically: `probe_clear_shared` (checkout, take an extra `shared_ptr` copy as the committer's `weak_ptr::lock()` would, checkin, `clear()`: the shard must stay, not be busy, be reused by the next checkout, and be freed after the extra copy is released). Branch: writable and readable both pass. MB: writable `kept=0`. Add it as a test.
2. M8, MD and ME need a clear in the window between `lk.unlock()`/`do_close` and `lk.lock()` while a waiter has been woken by a checkin and loses the race for `mtx`. On an unopened shard `do_close` has no blocking primitive (`shard.cc:764-810`), so a deterministic test needs a seam (a test hook in `clear()` between unlock and relock, or a real opened shard with a pending commit to lengthen `do_close`). I could not build one from outside; answer to the lead's question: not feasible as a unit test without a seam; keep the hammer (`stallcount.sh`) as the check and add the seam as a follow-up. For my runs: hammer 10 rounds pool off, 0 timeouts, 0 ASan reports.

### CR3. `use_count() == 1` does not exclude `weak_ptr::lock()`; the drop is deferred to the end of `clear()` (non-blocking, follow-up with the unfixed problem 2)

`pool.cc:481-486` comment says nobody can copy a busy slot's pointer. True of the slot, not of `weak_ptr<Shard>`: `Shard::autocommit` (`shard.cc:853-866`) debounces `committer_commit(weak_ptr<Shard>)`, which does `weak_shard.lock()` then `Endpoint(shard->endpoint)` (`handler.cc:2913-2917`). Master's clear re-added the shard if `weak.lock()` found another owner. The branch moves the shard into `dropped` and destroys it at the very end of `clear()` (`pool.cc:522-524`), after every readable's `do_close`, so the window in which a committer can take a reference to a shard the endpoint no longer tracks is wider than master's. Once `clear()` returns, `cleanup` may evict the endpoint while the committer still holds the shard and reads `shard->endpoint`. This is the contributor's "not reproduced" problem 2; I did not reproduce it either (12 rounds of `uafhunt.sh`, 0 ASan reports). Ruling on that problem: follow-up, not fix-now. Cheapest fix when it is done: have the debounce capture the `Endpoint` by value (or `committer_commit` take a `ReferencedShardEndpoint` first) and destroy the dropped writable right after relocking instead of at the end. Correct the comment to say "no one can copy it except a `weak_ptr` lock".

### CR4. `ShardEndpoint::finish()` still notifies without `mtx` (non-blocking, follow-up)

`pool.cc:458-465`: `finished = true; writable_cond.notify_all(); readables_cond.notify_all();` without `mtx`. Same lost-wake-up shape as CR8 item 2: a waiter that checked `is_finished()` under `mtx` and has not yet blocked misses it and sleeps until its own timeout (shutdown only; `timeout < 0` waiters wake every second). `DatabasePool::finish()` calls it holding the pool mutex, so a fix must not lock the endpoint mutex under the pool mutex while a waiter can hold the endpoint mutex and call `database_pool.is_locked` (see CR5 note 2): copy the endpoints out first. Similarly `checkin_clears_cond.notify_all()` is called without the pool mutex that `DatabasePool::join` waits on (`pool.cc:1080`); bounded by `join`'s deadline. Not probed.

### CR5. `cleanup_mtx`: no deadlock found; it extends the reactor-blocking exposure (non-blocking, record against engine-core CR1)

1. Lock order: `cleanup_mtx` is outermost. Only two callers take it (`DatabaseCleanup` loop `cleanup.cc:79` and `flush_view` `search_views.cc:1522`); neither holds a pool, endpoint or shard lock, and nothing under `cleanup` calls back into `cleanup`. `DatabasePool::lock()` calls `clear()` directly, not `cleanup`. checkin, the committer (`committer_commit`, which uses `lock_shard`) and RESTORE/DUMP paths never take `cleanup_mtx`. The new `lock_guard`s in `checkin` and `unlock` nest only `mtx` and call nothing that takes the pool mutex while holding it (`notify_lockable` runs before the lock in both branches).
2. Pre-existing inversion, not introduced: `cleanup`/`DatabasePool::finish` take pool mutex then endpoint mutex (`is_used`, `finish`), while a waiter's predicate holds the endpoint mutex and calls `database_pool.is_locked(*this)`, which takes the pool mutex when `locks != 0` (`pool.cc:184`, `:288`, `:884-900`). It needs an exclusive lock in flight to bite. Record for the owner; the unfixed `lock()` path is where it would show.
3. Blocking: `cleanup_mtx` is held across `ShardEndpoint::clear()`, whose `do_close` commits (`task::sync_wait`). Master ran concurrent cleanups in parallel (that was the bug); now a `FLUSH` on a reactor thread (inline, `search_views.cc:1511-1531`) also waits for a periodic cleanup's commits. `mtx`/pool mutex are never held across a commit (checked in `clear()`, `cleanup`, `checkin`). It adds to engine-core CR1 (reactor blocked on commit) rather than creating it; no new rule is broken beyond that known one.

### CR6. The two problems left unfixed: rulings

1. `DatabasePool::clear()` (shutdown via `join`) deleting endpoints a concurrent caller references: follow-up. Shutdown only, after the cleanup thread is finished; cheap fix when done: take `cleanup_mtx` in `clear()` (outside the pool mutex) and skip `lru::clear()` while any `refs != 0`.
2. Committer reading `shard->endpoint` after eviction: follow-up, see CR3.

### Notes (non-blocking)

1. Reading `clear()` for the rest of the lead's checklist: no path leaves a shard reachable after its endpoint is freed (a kept shard keeps `is_used()` true so cleanup leaves the endpoint; a dropped shard is out of the slot under `mtx` and `use_count() == 1`, other than the weak-lock window in CR3); a shard that cleanup keeps is dropped by the next cleanup. `dropped` is destroyed outside `mtx` while callers hold a `ReferencedShardEndpoint`, so `~Shard` cannot see a freed endpoint. Waiters: every state change in `clear()` (`busy` taken, shard dropped, shard kept, accounting restored) is under `mtx` and followed by `notify_all` in the paths I read; MA/MD/ME show the tests do not pin that.
2. `do_close(..., throw_exceptions=false)` swallows commit errors, so a failed commit during `clear()` drops the shard (as master). If `do_close` ever threw, `clear()` would leave `_busy` set (master the same).
3. `DatabasePool::lock()` waits on `lockable_cond`, which only `checkin` signals (`notify_lockable`); a readable held by cleanup's `clear()` is never "checked in", so `lock()` can wait out its timeout. Pre-existing, not probed.
4. The TSan binary shows 12 checks (it includes the new clear tests); TSan cannot see CR1 (destroyed-mutex use inside libc++).
5. Scope: `pool.cc`/`pool.h`/`shard.cc` changes, the `PoolTestAccess` friends (test-only, three one-line friend declarations in headers) and the `pool` entry in the test list; nothing beyond the design. Changelog bullets are accurate for items 1, 2, 3 (the `clear()` rework is covered by the first bullet's scope; fine).

## Probes and artifacts (all in `.scratch/review/`)

1. `probe_drop.cc`, `drop_window.lldb`, `probe_drop`: CR1 deterministic.
2. `probe_drop_stress.cc`, `probe_drop_stress` (branch), `probe_drop_stress_fixed` (with `pool_fixed.cc`): CR1 race and fix.
3. `probe_clear_shared.cc`, `probe_clear_shared`, `probe_clear_shared_MB`: CR2 shared-shard rule.
4. `pool_MA.cc`, `pool_MB.cc`, `pool_MD.cc`, `pool_ME.cc`, `pool_MF.cc`, `runmut.sh`, `test_pool_M*`, `run_M*.log`, `build_M*.log`, `mut_summary.txt`: mutants and results.
5. `mkobj.sh`, `linkbin.sh`, `cmd_pool.cc.txt`: compile one TU with `build-asan` flags and relink `xapiand_test_pool`'s object list with swapped objects (reusable for any further mutant).
6. `uafhunt.sh`, `stallcount.sh`, `hammer.sh`, `node.sh` (copies; `node.sh` points at `build-asan/bin/xapiand`): server runs. `uafhunt.sh` 12 rounds, 16 clients with FLUSH: node alive, 0 ASan reports. `stallcount.sh 0 9102 10`: 0 rounds with a timeout, 0 ASan reports.

## Not run

TSan on the server binary and on my probes; the pool-on (`XAPIAND_ENGINE_OFFLOAD_THREADS`) hammer; the docs E2E (contributor reports GREEN, not re-run); the contributor's mutants M1-M9 (only my own MA-MF run; M8 equals MA); master comparison of CR1 (reasoned from the code: master's unlocked notify is silent); the cluster/remote paths; Linux and FreeBSD (no CI; `pthread_mutex_lock` on a destroyed mutex may not fail there, so CR1 is silent UB rather than an abort); a probe for `finish()`/`join` lost wake-ups (CR4) and for the committer window (CR3); anything under `src/xapian/` and the GPL files (not read).

## Re-check (2026-10-26)

Scope: CR1 and CR2 against fix commit a3890d79f (on 0ca4e013e). Diff read: `git diff 0ca4e013e a3890d79f` (`src/database/pool.cc`, `tests/test_pool.cc`, `docs/status/pool-concurrency.md`). Artifacts in `.scratch/review/recheck/`.

### CR1: CLOSED

1. `src/database/pool.cc:399-404` computes `drop` first (`is_finished() || notify_lockable(*this) || is_closed()`, then `drop = shard->is_closed()` after `Shard::autocommit`); the WAL delete stays before the lock (`:405-413`); `:417-424` is one `lock_guard` section doing `if (drop) { writable.reset(); checkin_clears_cond.notify_all(); }`, `_busy.store(false)`, `writable_cond.notify_one()`. Nothing in `checkin` locks `mtx` after that section (what follows is `shard.reset()` and the local `pending_callbacks.call()`, `:447-449`). The function is textually the reviewer's `pool_fixed.cc` plus comments (`diff` shows only comment lines).
2. No new double-lock or lock-order change. `mtx` is taken twice in `checkin` on the writable path: the callbacks swap (`:394-396`, released), then the one section (`:417`). `is_finished()` and `notify_lockable(*this)` still run with `mtx` released, as on 0ca4e013e. `checkin_clears_cond.notify_all()` is still under `mtx` and takes no pool mutex. The readable branch is untouched. One ordering change: on a dropping restore shard `delete_wal` now runs before `writable.reset()` (the slot still holds the shard, `_busy` still set); benign, and what the reviewer's fixed copy did.
3. `probe_drop` (`recheck/probe_drop_fixed`, linked against the fixed `pool.cc.o`): `drop_window.lldb`'s breakpoint at `pool.cc:423` no longer marks a gap (it now sits inside the single section; running `cleanup` there would self-deadlock on `mtx`), so I ran the same script at the two points where the endpoint is exposed: `:417` (decision made, section not yet entered) and `:447` (`shard.reset()`, after the section). Both: `cleanup(true,false)` returns, `checkin returned`, exit 0, no abort (`recheck/lldb_fixed.log`). At `:417` the writable slot still holds the shard, so the endpoint is not evictable; the gap is gone.
4. `probe_drop_stress` (`recheck/probe_drop_stress_fixed`, 10 s, 3 runs): survived 522630, 487367 and 486161 checkins, rc 0 each. Control, same source linked against the old object (`../probe_drop_stress`, branch before the fix): `mutex lock failed: Invalid argument`, abort, rc 134 (`recheck/stress_fixed.log`).
5. Revert check: `recheck/pool_oldcheckin.cc` (= `git show 0ca4e013e:src/database/pool.cc`, only `checkin` differs from the fix) linked with the fix's `test_pool.cc.o`: `libc++abi: terminating due to uncaught exception of type std::__1::system_error: mutex lock failed: Invalid argument`, rc 134 (`recheck/run_oldcheckin.log`). With the fix: ASan `xapiand_test_pool` 21 checks passed; TSan build 21 checks passed, 0 warnings. `test_checkin_drop_races_cleanup` (`tests/test_pool.cc:216-247`) is the stress shape (2 writers closing and checking in on their own endpoints, 3 `cleanup(true,false)` threads, 5 s) and the only new test that can reach the path. Test cost: 5 s of wall time.

### CR2: CLOSED

1. `test_clear_keeps_shared_writable` and `test_clear_keeps_shared_readable` exist (`tests/test_pool.cc:269-270`, helper `:250-267`, both registered in `TEST_MAIN`). They are the `probe_clear_shared` sequence: checkout, extra `shared_ptr`, checkin, `clear()` returns 1 kept, shard not busy, next checkout returns the same shard, `clear()` returns 0 after the extra reference and the shard are released.
2. Mutants built from the fixed `pool.cc` (`recheck/pool_MBw.cc`: `clear()` writable drop condition `use_count() == 1` replaced by `true`; `recheck/pool_MBr.cc`: the same for the readable condition), linked with the fix's test object, ASan flags:
   - MBw: 3 of 21 checks FAILED (`test_pool.cc:258`, `:259`, `:261`), rc 1.
   - MBr: 3 of 21 checks FAILED (same lines), rc 1. This is the variant the contributor did not run. Each mutant fails exactly one test's three checks (not six), so each test kills its own mutant independently.
   - Unmutated fix: 21 passed.
3. Still unpinned, as the review said (no seam): MA, MD, ME (`clear()` wake-ups). Not part of the condition.

### New findings

None from the diff. One line outside the scope, for the lead (not verified, pre-existing): `checkin` still ends with `shard.reset()` (`pool.cc:447`) after the endpoint is evictable; on a dropped open shard `~Shard` runs `do_close` there, which I did not trace for uses of `shard->endpoint` (the existing CR3 caveat about who owns the endpoint's lifetime across a dropped shard's destructor).

### Not run

Server binary under ASan/TSan and the hammer (`stallcount.sh`, `uafhunt.sh`) against the fix; the docs E2E; pool-on hammer; TSan builds of the probes and of the mutants; the old-checkin revert check was done by object swap (old `pool.cc` copy linked into `test_pool`), not by editing the worktree; Linux and FreeBSD; `clear()` mutants MA, MD, ME; CR3 to CR5 and the notes (not in scope). Nothing under `src/xapian/` or the GPL files was read.
