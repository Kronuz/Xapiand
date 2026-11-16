# t11-barriers: journal

State: landed
Quest: .campaign/xapiand/quests/t11-barriers.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:7125dfc28 on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/t11-barriers.md`: "E1 fold the first directory barrier into the data batch: completion `cdf1cb3`, strata `0765e28`." |
| A2 | `docs/status/t11-barriers.md`: "E2 order the manifest temporary with a barrier: completion `5a338d8`, strata `307d08b`" |
| A3 | `docs/status/t11-barriers.md`: "E3 one ordering point and one flush: completion `fdfb94f`, strata `d01a1c4`." |
| A4 | `docs/status/t11-barriers.md`: "strata faultfs models ordering points: strata `67671ec`." |
| A5 | `docs/status/t11-barriers.md`: "Cleanup, docs and pins: completion `3665ee7`, strata `bbfb986` `cccb2f9`, prospect `23207cc`" |
| A6 | `docs/status/t11-barriers.md`: "TSan race fix (the commit copied a whole segment ref while a merge read it): strata `e471404`" |
| A7 | `docs/status/t11-barriers.md`: "L1 (CB-1): `DeviceSyncCalls::barrier` (`posix.h`) falls back to `F_FULLFSYNC` only on `ENOTSUP`" |
| A8 | `docs/status/t11-barriers.md`: "L2 (CB-2): `PosixIO::replace_sync_calls(fsync, fcntl)` (macOS) routes every sync call" |
| A9 | `docs/status/t11-barriers.md`: "C1 (CB-3): completion README, ARCHITECTURE and `docs/PLATFORM_FINDINGS.md` say the barrier orders, is not a flush" |

## Result

Landed at Xapiand:7125dfc28 on 2026-10-29. T11 CPU row: 0.372x Xapian, pairs 0.355 to 0.408; master's code 0.465x, A/A 0.478x. Review: strata `docs/reviews/commit-barriers-durability.md` -- REJECT on CB-1 and CB-2, then its re-check (2026-10-29) CLOSED L1 (CB-1), L2 (CB-2) and C1 (CB-3). C2 (CB-4, the macOS TSan blind spot in `calibration.md`) and C3 (CB-5, faultfs post-mark writes) were not re-checked and stay open: quest `barrier-warning-strata` A5, A6.

- Merge: Merge branch 'phase7a/t11-barriers' (one ordering point and one flush per commit).
- Completion `cdf1cb3`, strata `0765e28` for E1; completion `5a338d8`, strata `307d08b` for E2.
- Completion `fdfb94f`, strata `d01a1c4` for E3.
- Strata `e471404`, prospect `b9b3c02`, Xapiand pin `d7d98d74e` fix the TSan race.
- Review fixes: completion `03cef1d`, strata `16e9054`, prospect `828cd4a`.
- Review fix suites: completion 17 of 17; strata suites 32 of 32; engine crash check PASS on both engines.
- No performance change landed; open: main-branch T11 rerun, C2, C3, non-Apple platforms and +3% peak RSS (the re-check closed CB-1 and CB-2).
- Docs: `docs/perf/t11-barriers/` named.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
