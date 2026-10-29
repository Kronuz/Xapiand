> Status: CURRENT (2026-10-29) gate 7a, blocker L2 and conditions C1, C2, C4 of `docs/reviews/phase-7a-gate.md` (strata); branch `phase7a/gate-l2` from master `3bc11f31f`; workstream gate7a-l2, developer.

# Gate 7a: L2 (every-gate checks) and C1, C2, C4

## State

In progress. C1 and C2 are done (harness and report); C4 is traced and its control run is in; the sanitizer, T10 and fuzz runs
are under way. Results go to "Every-gate checks" in `docs/gates/phase7a.md`.

## Done (commits)

- Xapiand:db2a9d813 `profile_bulk.sh`: device-flush wait as its own blocked line; truncated profiles rejected (C1, C2)
- Xapiand:bf25fb518 `harness/engine_mixed_load.py`: the T10 mixed index and search load

## Next

Every-gate checks (T10 under TSan, ASan+UBSan ctest and engine E2E, fuzz, coverage statement); the C4 report.

## Blocked on

Nothing.
