# loop-yield-race: journal

State: waiting on jobs 261118-085546-726c47 261118-085535-5b48c7 261118-085558-3ca281 261118-085607-269ff5
Quest: .campaign/xapiand/quests/loop-yield-race.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix committed (9d56c48f8); jobs queued for A1-A3.

## Next

1. Read jobs 261118-085546-726c47 (A1 x20 TSan), 261118-085535-5b48c7 (A1 mutant, fails-without),
   261118-085558-3ca281 (build-test, A3), 261118-085607-269ff5 (A2, 4 burners); paste `campaign brief`'s table.
2. Lead: push the branch for CI (A3).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending 261118-085546-726c47; mutant 261118-085535-5b48c7 at mutant/loop-yield-race-a1 (0da1c4d51) |
| A2 | pending 261118-085607-269ff5 |
| A3 | pending 261118-085558-3ca281; CI not yet pushed |

## Result

1. TSan race (CI 44056420626): `count_delete` (called from operator delete on every thread) read the
   plain global `counted_thread` after a relaxed load of `counting`; a DatabasePool timer freeing on
   the manager loop (T7) had no order to the HTTP reactor thread (T10) that wrote it at :1239.
   The race is in the test, not src/. Fix: `counted_thread` is `std::atomic<pthread_t>`, read after an
   acquire load of `counting` (the seq_cst store of `true` follows the thread's store). The test now
   forces the interleaving: a thread with only a relaxed flag ordering it to the loop calls
   `count_delete()` directly once counting starts (direct call, since clang TSan bypasses a test's own
   operator delete). The mutant keeps the probe and reverts the fix.
2. test_view_holds_nothing (`test_engine_search_strata.cc`): the `elapsed < 800 ms` bound and the
   1 s hold are gone. The spy is released only after every writer returns, so a write that waited
   for the search would never return; the test counts the 15 writes and checks the search was still
   inside its first see() (`spy.seen == 1`) when they finished. No time bound remains in that test.

## Rulings I made

1. No watchdog replaces the 800 ms bound: a regression (a write waiting for the search) deadlocks
   and ctest's timeout reports it; any watchdog would be another load-sensitive bound.
2. TSan evidence is local macOS clang (llvm@22), RelWithDebInfo, own build-tsan and .deps-tsan;
   Linux gcc TSan is CI's.
3. The 10 s bound in test_huge_window (:1481) is outside this quest; left alone.
