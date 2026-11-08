# phase7c-s1: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-s1.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:f3bd508f8 on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c.md`: "[x] **K1 reactor `LagProbe`** (`reactor:3041327`, `reactor:ef26c30`" |
| A2 | `docs/status/phase7c.md`: "[x] **K2 probes in Xapiand** (`Xapiand:56c20326e`, pins at `reactor:ef26c30`, `http:e5b9cd9`)." |
| A3 | `docs/status/phase7c.md`: "[x] **K3 `harness/stall_load.py`** (C3). The RESTORE writes `restore-target`" |
| A4 | `docs/status/phase7c.md`: "[x] **K4 `harness/thread_census.sh`**: names and counts by prefix" |
| A5 | `docs/status/phase7c.md`: "[x] **K5 `harness/async_check.sh`** (C1): rules for `sync_wait`, waits and condition variables" |
| A6 | `docs/status/phase7c.md`: "[x] **K6 `harness/profile_light.sh`** (C2) and `http` `trace/hop` (`http:10c5074`, CI green)" |
| A7 | `docs/status/phase7c.md`: "[x] **K7 the commands the lead runs** for T5, T11 and T12 on `main` before X2 lands" |

## Result

Landed at Xapiand:f3bd508f8 on 2026-10-29. Docs E2E: 649 of 649 assertions, 0 failing; 377 requests. Review: `docs/reviews/phase7c-design.md` -- APPROVE WITH CONDITIONS for S1 only (preliminary).

- Merge: Merge branch 'phase7c/s1-tooling' (7c gate tooling and the pre-7c baseline).
- reactor:3041327, reactor:ef26c30; http:e3d5236, http:e5b9cd9, http:10c5074; Xapiand:56c20326e and listed tooling commits.
- Reactor CI: all 8 jobs green on reactor:ef26c30.
- 0 unclassified hits on this tree; 316 hits in src/ (96 allowed, 207 owed, 13 compiled out).
- Pre-7c light query p99 at 210 requests a second: 70.6 ms unloaded; about half of light requests do not finish in 30 s under heavy load.
- HTTP-loop lag p99 is at most 896 us under heavy load; control-loop lag p99 is 2,560 us even idle.
- Dispatch per light request: 18.7 us CPU, 25.2 us wall; RESTORE 6,076 docs/s; 16-client queries 1,277 QPS.
- Baseline is preliminary; lead re-measurement on main remains open.
- Docs: `docs/perf/phase7c-baseline.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
