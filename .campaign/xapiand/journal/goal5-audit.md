# goal5-audit: journal

State: waiting on jobs 261118-030453-e7e654
Quest: .campaign/xapiand/quests/goal5-audit.md (the lead's; never edit it)

## Doing

Nothing: the audit is committed (consensus:42cb3cf); the A2 grep job runs at it.

## Next

1. When 261118-030453-e7e654 passes, paste `campaign brief`'s table and set ready for review.

## Blocked on

Nothing. Two findings for the lead to file as library quests (below).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/GOAL5-AUDIT.md` in consensus at consensus:42cb3cf: completion:d8b538c6778f, journal:ce05f0efd411, consensus:b7f48b2414d8, task:95798a02f68c, wheel:238b1ae9f58b |
| A2 | job 261118-030453-e7e654 (light): the doc's embedded grep, 70 sites, each must match a row; fails on a missing row (checked by hand: deleting one row's token gives "1 without an audit row", exit 1) |

Lead's mid-task message (Detent quests change Driver/Worker close): read consensus-close-fenced and
consensus-worker-release; rows CN6, CN7 and JN7 are marked "changing in detent:<quest>", not findings.

## Result

consensus `docs/GOAL5-AUDIT.md`: rows per library: completion 3, journal 9, consensus 17, task 10,
wheel 2 (41 rows over 70 grep sites). No tracing collector or collector thread anywhere.
Findings (no bound):
- G5-J1 journal: `TurnDriver::forget` (turn_driver.h:145) adds a `retired_` floor per closed group id; nothing erases it.
- G5-C1 consensus: `Lookup::start` (resolve.h:99) detaches an unowned resolver thread; `abandon()` stops delivery only, so the count across transport rebuilds is bounded only by the resolver's timeout.
Changing in Detent: CN6 (consensus-close-fenced), CN7 (consensus-worker-release), JN7 (journal-reopen-closed).
Observations (bounded): O1 task frame pool 33 MiB/thread worst case; O2 TurnLog::reclaim has no fixed per-turn segment count; O3 completion fallback thread even with native io_uring; O4 Host::group_io sweeps all ranges_ per call.

## Rulings I made

- Scope is library code (include/, top-level headers); tests, benches, fuzz, examples, tools excluded.
- Spawned task roots count as deferred-destruction sites (teardown destroys suspended ones), so each spawn site has a row.
- A2 is self-checking: the grep lives in the doc and fails when a hit lacks a backticked `lib:path:line` row token.
- Bounded but large or uncapped-per-slice paths are Observations, not findings.
