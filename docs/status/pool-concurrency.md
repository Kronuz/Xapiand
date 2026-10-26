# pool-concurrency (CR8)

Branch `fix/pool-concurrency` from Xapiand master `c625977e7`, worktree
`Xapiand-wt/pool-concurrency`. Source: `docs/reviews/engine-core.md` CR8. Concurrency tier: code
review before the lead merges. Commits stay local.

## State

- State: in progress (fixes written, verifying).
- Next: verify fix v2 (pool test, UAF hunt, reopen count), revert checks, TSan, docs E2E.
- Blocked on: nothing.

## Done criteria

- [ ] K1 Each bug reproduced on master (command and output), root cause with file:line.
- [ ] K2 Each fix has a test or repro that fails before and passes after (revert check).
- [ ] K3 UAF repro clean under ASan for 3x the iterations it took on master; lost wake-up has a
  deterministic or stress test; reopen succeeds in one pass.
- [ ] K4 TSan (Homebrew LLVM) on the pool tests; docs E2E passes as on master.
- [ ] K5 Changelog bullet under `[Unreleased]` if user-visible.

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
  Fix: both update and notify under the endpoint `mtx`. Notifying under `mtx` also keeps the
  endpoint alive until checkin is done with it (cleanup needs `mtx` to see the shard as free).
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
