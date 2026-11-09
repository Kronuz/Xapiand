# scheduler-unify-pool: journal

State: landed
Quest: .campaign/xapiand/quests/scheduler-unify-pool.md (the lead's; never edit it)

## Doing

Nothing: landed at scheduler:1e5ccc9 on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/unify-pool.md`: "Tests: `ctest` (RelWithDebInfo, asserts live): scheduler, scheduler_no_memory pass." |
| A2 | `docs/status/unify-pool.md`: "Mutants (`.scratch/mut.sh`, each fails the test): finish keeps queued tasks" |
| A3 | `docs/status/unify-pool.md`: "new is within the control's spread. Firing a backlog through the pool is 5.7x faster." |
| A4 | `docs/status/unify-pool.md`: "Sanitizers: `preflight.sh` (mac hardened, mac ASan+UBSan, Linux and FreeBSD VMs) ok on both repos" |
| A5 | `docs/status/unify-pool.md`: "threadpool run 41542646368 and scheduler run 41542683108, all jobs green." |
| A6 | `docs/status/unify-pool.md`: "CR1: merged `origin/main` (conflicts in `AGENTS.md`, `CMakeLists.txt`); ctest 8/8" |
| A7 | `docs/status/unify-pool.md`: "`CMakeLists.txt` pins threadpool `eb4751a` (branch `phase7c/drop-threadpool`, which deletes" |

## Result

Landed at scheduler:1e5ccc9 on 2026-11-02. Merged at scheduler:d94f287, then the drop-threadpool pin scheduler:1e5ccc9.

- Review: `docs/reviews/unify-pool-code.md` -- APPROVE WITH CONDITIONS.
- Raw A/B committed in `docs/status/unify-pool/ab-main.txt` and `ab.txt`.
- Docs: `docs/status/unify-pool/ab-main.txt`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
