# pool-concurrency (CR8)

Branch `fix/pool-concurrency` from Xapiand master `c625977e7`, worktree
`Xapiand-wt/pool-concurrency`. Source: `docs/reviews/engine-core.md` CR8. Concurrency tier: code
review before the lead merges. Commits stay local.

## State

- State: in review (code reviewer, concurrency tier). Commits are local, not pushed.
- Commits: `cd5f7b44e` tests, `28f6b88ff` pool fixes (CR8 1, 2), `df87f4e1c` reopen (CR8 3),
  `b8a286309` changelog and ledger, `041187739` clear() wake-up tests, plus this ledger update.
- Next: code review; then the lead merges.
- Blocked on: nothing. Scope note for the lead: fixing item 1 exposed `ShardEndpoint::clear()`
  bugs (below) that crash the same FLUSH repro, so clear() was reworked too; CR8 asked for a
  designer or pool owner on checkout and cleanup concurrency, so the reviewer should weigh that.

## Done criteria

- [x] K1 Reproduced on master (below, 2026-10-25): UAF in round 1 of `uafhunt.sh` (4 of 4 runs)
  and in `test_concurrent_cleanup`; lost wake-ups by three deterministic tests and the hammer
  (6 of 30 rounds with a timeout); reopen by lldb counts (11 opens per reopen). Root causes with
  file:line in the log.
- [x] K2 Each fix has a failing-before test; revert checks and mutants in the table below.
- [x] K3 UAF hunt clean for 30 rounds (master: round 1, so 30 times); lost wake-up: deterministic
  tests plus hammer 0 of 30 rounds (master 6 of 30); reopen opens once (lldb 11 vs 121 writable
  opens for the same run).
- [x] K4 TSan (Homebrew LLVM 22): `test_pool` 3 runs, 0 warnings, 12 checks; docs E2E GREEN on
  master and fix, 0 ASan reports.
- [x] K5 Two bullets under `[Unreleased]` / Fixed.

## Revert checks and mutants

`.scratch/mutrun.sh` applies each mutant (`.scratch/mutants.py`), rebuilds `test_pool`, runs it,
restores. Logs in `.scratch/test_pool-M*.log`.

| Mutant | Breaks | Caught by |
|---|---|---|
| M1 | no `cleanup_mtx` | `test_concurrent_cleanup`: ASan heap-use-after-free |
| M2 | checkin writable releases outside `mtx` | `test_checkin_wakes_writable_waiter` FAIL |
| M3 | checkin readable releases outside `mtx` | `test_checkin_wakes_readable_waiter` FAIL |
| M4 | unlock notifies outside `mtx` | `test_unlock_wakes_readable_waiter` FAIL |
| M5 | master's `clear()` | `test_concurrent_cleanup`: ASan heap-use-after-free `pool.cc:437` (checkin on an evicted endpoint) |
| M6 | predicate dereferences a null `writable` | `test_concurrent_cleanup`: UBSan null dereference |
| M7 | clear leaves the readable it holds counted | `test_clear_wakes_readable_waiters`: hung (watchdog) |
| M8 | clear does not wake writable waiters | not by the unit test (clear's window is too short on an unopened shard); hammer 7 of 30 rounds with a timeout (fix 0 of 30) |
| M9 | no `break` in `Shard::reopen` | lldb: 121 writable and 286 readable opens (fix 11 and 25) |

## Found, not fixed (pre-existing, for the reviewer or owner)

- `DatabasePool::clear()` (shutdown, via `join`) clears the LRU without checking `refs`, so an
  endpoint a concurrent caller still references is deleted. Shutdown only.
- A writable that checkin drops (`writable.reset()` when closed, finished or lockable) can stay
  alive with the committer, which then reads `shard->endpoint` after the endpoint may have been
  evicted (`committer_commit`, `handler.cc:2913`). Not reproduced.
- Toolchain: Homebrew LLVM 22 and 23 do not build Xapiand as is (SDK include dirs from
  `UUID_INCLUDE_DIR`/`ZLIB_INCLUDE_DIR`, the pinned RapidJSON's const assignment, and
  `XXH_STATIC_LINKING_ONLY` redefined under `-Werror`); details in the log.

## Log

### 2026-10-25

- Read CR8. Copied `uafhunt.sh`, `hammer.sh`, `stallcount.sh`, `node.sh` and `uaf-8987.log` from
  the main checkout's `.scratch/engine-core-review/` into `.scratch/`.
- Added `tests/test_pool.cc` (deterministic wake-up tests and a concurrent-cleanup stress test) and a
  `PoolTestAccess` friend in `src/database/pool.h`.

- ASan+UBSan build (`build-asan`, AppleClang, RelWithDebInfo, `-DLTO=OFF -DBUILD_TESTS=ON`, as the
  engine-core review). Master binaries kept in `.scratch/master-bin/`.
- Master, `tests/test_pool.cc` (`.scratch/test_pool-master.log`): the three wake-up tests FAIL
  (checkin writable, checkin readable, unlock), and the concurrent-cleanup test aborts with ASan
  heap-use-after-free at `lru.h:107` from `DatabasePool::cleanup` `pool.cc:1106`, freed by another
  cleanup's evict: the same report as CR8.
- Master, `uafhunt.sh master-bin/xapiand 8991 200`: heap-use-after-free `lru.h:107` in
  `DatabasePool::cleanup` from `flush_view`, in round 1, 4 runs of 4 (`.scratch/uaf-master-run*.log`).
- Root cause 1: `DatabasePool::cleanup` (`pool.cc:1096-1150`) walks the LRU with `trim_and`, whose
  iterator already points at the next node, and its callback drops `mtx` to clear the endpoint.
  Another cleanup evicts and frees that next node meanwhile. Fix: `cleanup_mtx` serialises
  cleanups (the only runtime evictors; `clear()` runs at shutdown).
- With 1 fixed, the hunt reached round 16 and hit `SEGV pool.cc:184` (`_writable_checkout`'s
  predicate dereferencing a null `writable`), and the unit test hit a heap-use-after-free in
  `ShardEndpoint::checkin` on an evicted endpoint. Root cause: `ShardEndpoint::clear()`
  (`pool.cc:469-535`) empties the slot (`writable.reset()`, `readable.reset()`) while it drops
  `mtx` to delete the shard. A checkout fills the empty slot meanwhile (`_readable_checkout`'s
  `has_empty` path, `_writable_checkout`'s `if (!writable)`), and clear then erases or overwrites
  that new shard, so its endpoint can be evicted while it is checked out; a waiter woken in the
  window dereferences the null `writable`. clear also never notified waiters, and left a readable
  it holds counted in `readables_available`, so a reader at the reader limit spins in
  `_readable_checkout` holding `mtx` and clear can never relock. Fix: the slot keeps the busy
  shard while clear works, `use_count() == 1` under `mtx` decides the drop (nobody can copy a busy
  slot's pointer), deletion happens after the walk outside `mtx`, the accounting tracks the shard
  clear holds, and clear notifies. The writable predicate treats a null slot as free.
- Root cause 2: `ShardEndpoint::checkin` stores `_busy = false` (and `++readables_available`)
  and notifies without `mtx` (`pool.cc:422-423`, `436-438`), so a waiter between its predicate
  check and its wait misses it. `DatabasePool::unlock` has the same shape (`pool.cc:303-304`).
  Fix: both update and notify under the endpoint `mtx`. (Correction, CR1: this keeps the
  endpoint alive only on the keep path and for readables. The writable drop path took `mtx` twice,
  so cleanup could evict the endpoint between the sections; fixed in the CR1 commit below.)
- Root cause 3: `Shard::reopen` (`shard.cc:663-690`) has no `break` after a successful
  `reopen_writable()`/`reopen_readable()`, so it reopens `DB_RETRIES + 1` = 11 times. Fix: `break`.
- Fix v2 (`28f6b88ff`, `df87f4e1c`; `.scratch/verify-fix2.out`): `test_pool` OK (10 checks),
  `uafhunt.sh build-asan/bin/xapiand 8994 30`: 30 rounds, node alive, 0 ASan reports (master hit
  in round 1 of 4 runs of 4, so 30 rounds is 30 times the master count).
- Reopen counts (`.scratch/reopenlldb.sh`, lldb auto-continue breakpoints, one PUT with commit
  and one GET on a fresh node, then SIGTERM): master `reopen_writable` 121 and
  `reopen_readable` 286 hits; fix 11 and 25. Before shutdown (first master run): 11 and 22, that
  is 11 opens per reopen. `.scratch/reopencount.sh` (WAL replay lines) was no proof: the replay
  line prints only when the WAL has ops past the revision, and both binaries print one per WAL.
- Docs E2E (`.scratch/e2e.sh`: `harness/e2e_capture.sh`, newman 6, `e2e_assert.py`,
  `e2e_diff.py` against the X1 final capture), ASan binaries: master GREEN/GREEN, fix GREEN/GREEN,
  0 ASan reports in each.
- Changelog: two bullets under `[Unreleased]` / Fixed.
- TSan build (`build-tsan`): Homebrew LLVM 23 and 22 do not build Xapiand as is. Libc++ rejects
  `-I<SDK>/usr/include`, which `UUID_INCLUDE_DIR` and `ZLIB_INCLUDE_DIR` resolve to (worked
  around with `.scratch/sdkinc/` symlinks); the pinned RapidJSON's `GenericStringRef::operator=`
  assigns a const member (patched in `build-tsan/_deps` only); `config.h:289` redefines
  `XXH_STATIC_LINKING_ONLY` under `-Werror` (`-Wno-macro-redefined`). Toolchain findings, not
  fixed here.

### 2026-10-26

- Mutants M1 to M9 (table above). M7 and M8 were missed by the first tests; added
  `test_clear_wakes_{writable,readable}_waiters` (`041187739`), which catch M7; M8 is caught by the
  hammer only.
- Hammer (`stallcount.sh 0 8997 30`, ASan binaries, pool off): master 6 of 30 rounds with a 20 s
  timeout, fix 0 of 30, M8 7 of 30. No ASan report in any.
- TSan rerun with the new tests: 3 runs, 0 warnings, 12 checks.
- No server left running; `build-asan` and `build-tsan` kept for the reviewer (rebuildable).

### 2026-10-26, review fixes (`docs/reviews/pool-concurrency-code.md`)

- CR1: the writable drop path of `ShardEndpoint::checkin` computes `drop` first (including the
  post-autocommit `is_closed()`), then does `writable.reset()`, `_busy = false` and `notify_one()`
  in one `mtx` section. Test `test_checkin_drop_races_cleanup` (2 threads close and check in a
  writable, 3 in `cleanup`, 5 s). Revert check: with the old `checkin` and the new test,
  `test_pool` aborts 3 of 3 runs (`mutex lock failed: Invalid argument`); with the fix it passes.
- CR2: `test_clear_keeps_shared_{writable,readable}` (probe_clear_shared). Mutant MB (clear drops
  the writable while shared): the writable test fails; unmutated, both pass.
- Still not unit-testable (needs a seam in `clear()` between unlock and relock): M8/MA, MD, ME.
  Hammer only.

## Follow-ups (from the review, not fixed here)

- CR3: `use_count() == 1` does not exclude `weak_ptr<Shard>::lock()` (the committer, `shard.cc`
  autocommit debounce, `handler.cc:2913`). Fix: capture the `Endpoint` by value in the debounce (or
  take a `ReferencedShardEndpoint` first) and destroy the dropped writable right after relocking;
  correct the comment at `pool.cc` "nobody can copy a busy slot's pointer". Same as the unfixed
  committer problem above.
- CR4: `ShardEndpoint::finish()` notifies without `mtx` (shutdown lost wake-up; copy endpoints out
  of the pool mutex before locking them); `checkin_clears_cond.notify_all()` is called without the
  pool mutex that `DatabasePool::join` waits on.
- CR5: record for the owner: `cleanup_mtx` is held across `clear()`'s commits, so a FLUSH on a
  reactor thread waits for a periodic cleanup (adds to engine-core CR1); the pool mutex then
  endpoint mutex order in `cleanup`/`finish` is inverted by a waiter predicate calling
  `database_pool.is_locked` (pre-existing, needs an exclusive lock in flight).
- CR6: `DatabasePool::clear()` at shutdown deletes endpoints a caller references: take
  `cleanup_mtx` outside the pool mutex and skip `lru::clear()` while any `refs != 0`.
- M8 seam: a test hook in `ShardEndpoint::clear()` between `lk.unlock()` and `lk.lock()` would
  let a unit test pin MA/M8, MD and ME.

### 2026-10-26 (lead)

- Re-check: CR1 and CR2 CLOSED (`docs/reviews/pool-concurrency-code.md`, "Re-check").
- Follow-up CR7 (lead, from the re-check's note): in `checkin`'s drop path the local `shard.reset()`
  (`src/database/pool.cc:447`) can drop the last reference after the endpoint is evictable, and
  `~Shard` runs `do_close` on a `Shard` that holds `ShardEndpoint&`. Pre-existing ordering; the
  only endpoint use seen in `do_close` is its `L_CALL` (off by default); its callees are untraced.
  Needs a probe before it is ruled out.
- Merged into local master.

