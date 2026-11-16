# threadpool-research-scheduler: journal

State: landed
Quest: .campaign/xapiand/quests/threadpool-research-scheduler.md (the lead's; never edit it)

## Doing

Nothing: landed at threadpool:394aaea on 2026-10-18 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/research-scheduler.md`: "[x] K1 The bound: per-task overhead and wake-latency floors on this hardware" |
| A2 | `docs/status/research-scheduler.md`: "[x] K2 A survey, reading source freely: work-stealing deques (Chase-Lev and its C11" |
| A3 | `docs/status/research-scheduler.md`: "[x] K3 At least three designs prototyped under `research/` (off by default)" |
| A4 | `docs/status/research-scheduler.md`: "[x] K4 The workloads measured with one hot group, 1,000 busy groups, and 10,000 groups" |
| A5 | `docs/status/research-scheduler.md`: "[x] K5 `docs/research/scheduler.md`: the bound, the survey with sources, each idea with" |
| A6 | `docs/status/research-scheduler.md`: "[x] K6 The branch pushed, CI green on all jobs: run 39137948331 (`workflow_dispatch`;" |

## Result

Landed at threadpool:394aaea on 2026-10-18.

- Query p50 at 1,000 busy groups: today 70 ms (Mac) against 65 to 71 us for the group and deque pools; idle CPU 0.35 to 0.47% of a core against 0.001 to 0.011%.
- Review: none recorded. Raw results committed in `docs/research/scheduler/` (33 files).
- Docs: `docs/research/scheduler.md`, `docs/research/scheduler/bound-mac.txt`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
