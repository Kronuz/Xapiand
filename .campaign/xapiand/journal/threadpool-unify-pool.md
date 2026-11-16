# threadpool-unify-pool: journal

State: landed
Quest: .campaign/xapiand/quests/threadpool-unify-pool.md (the lead's; never edit it)

## Doing

Nothing: landed at threadpool:3f1fbcf on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/unify-pool.md`: "`WorkGroup::pending()` / `running()` with `test/work_counts.cc`; mutants in `test/mutants.sh`:" |
| A2 | `docs/status/unify-pool.md`: "Deleted: `threadpool.hh` (`ThreadPool`, `ThreadPoolThread`, `PackagedTask`, `TaskWrapper`, `TaskQueue`)" |
| A3 | `docs/status/unify-pool.md`: "Proof: ctest 6/6 (RelWithDebInfo, asserts live); `test/mutants.sh` both counter mutants caught." |
| A4 | `docs/status/unify-pool.md`: "Repins (this wave), each on its repo's `phase7c/drop-threadpool`, CI green on the head named:" |
| A5 | `docs/status/unify-pool.md`: "commits `Xapiand:602121ecd` and `Xapiand:45d81215e` (fs `291842c`). Release ctest 32/32, ASan+UBSan ctest 32/32," |

## Result

Landed at threadpool:3f1fbcf on 2026-11-02. Step 1 threadpool:0983f2d, step 3 merged from `phase7c/drop-threadpool`; the ledger's last commit threadpool:3f1fbcf records the merged-tree Xapiand proof.

- Companion quests: `scheduler-unify-pool`, `logger-drop-threadpool`, `prospect-drop-threadpool`, and strata's `drop-threadpool` (strata:864ea69).
- Review: none in threadpool; the scheduler half's `docs/reviews/unify-pool-code.md` (scheduler) -- APPROVE WITH CONDITIONS.
- Docs: `docs/API.md`, `docs/mechanisms/work-pool.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
