# loop-yield-tsan: journal

State: waiting on jobs 261119-175247-5b4f11 261119-175249-f4278e 261119-175253-76f484
Quest: .campaign/xapiand/quests/loop-yield-tsan.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Cause found; the fix is already in this branch's base (xapiand:9d56c48f8, quest loop-yield-race), so this
quest adds no source change. Jobs queued at 2fee0fee3 to prove it again here.

Cause (A1), CI 44056420626 at 37eda4ed0 (and 44344005112 at f730dfa30, whose base lacks 9d56c48f8):
`count_delete` (tests/test_loop_yield.cc:78, called from operator delete on every thread) read the plain
global `counted_thread` after a relaxed load of `counting`. The manager loop thread T7 freed a
DatabasePool timer node (pool.cc:471) with no order to the HTTP reactor thread T10 that wrote
`counted_thread` at :1239 (in `test_a_restores_objects_are_freed_off_its_loop`). Test-only: src/ is not
involved. Fix in 9d56c48f8: `std::atomic<pthread_t> counted_thread`, read after an acquire load of
`counting` (the writer's seq_cst store of `counting = true` follows its store of the thread), and a probe
thread that calls `count_delete()` once counting starts with only a relaxed flag ordering it, so the
interleaving is forced. No assertion changed. Both CI hits at 44344005112 are on a base without the fix.

## Next

1. Read the jobs below; paste `campaign brief`'s table; set "ready for review".
2. Lead: push the branch for CI (A3).

## Blocked on

Nothing. Note for the lead: loop-yield-race already holds this fix; this quest may be redundant (A3 CI is
the only open item, which loop-yield-race's own landing also gives).

## Evidence

Jobs: 261119-175247-5b4f11 (TSan x20 at the fix, A1/A2; macOS clang llvm@22 as in loop-yield-race),
261119-175249-f4278e (mutant c96d605ec = the fix reverted, probe kept, branch mutant/loop-yield-tsan-a1;
fails-without), 261119-175253-76f484 (suite).

| Criterion | Evidence |
|---|---|
| A1 | pending 261119-175253-76f484 (suite) and the cause above |
| A2 | pending 261119-175247-5b4f11; mutant 261119-175249-f4278e |
| A3 | CI not pushed |

## Result

Test-only fix already present (xapiand:9d56c48f8); no new source change here.

## Rulings I made

1. No new edit: re-fixing would duplicate 9d56c48f8. Evidence is re-run at this branch's commit.
2. TSan evidence is local macOS clang (llvm@22), as in loop-yield-race; Linux gcc TSan is CI's.
