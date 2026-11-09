# pressure-flush: journal

State: landed
Quest: .campaign/xapiand/quests/pressure-flush.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:5737867 on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/pressure-flush.md`: "`165985b` Option `pressure_flush_divisor` (default 4; 0 = flush on any pressure)." |
| A2 | `docs/status/pressure-flush.md`: "2000 applies of 200 bytes, share 256 KiB: 14 segments by default, 1999 with divisor 0." |
| A3 | `docs/status/pressure-flush.md`: "Ab tooling: `bench/kv/ab.py` gains the `pressure` and `pressure_more` row sets, and records a failed run" |

## Result

Landed at strata:5737867 on 2026-10-21. Reverting the gate or using the share/(4*250) mutant fails the boundedness check. No review named in the ledger.

- Merge: Merge remote-tracking branch 'origin/phase4/pressure-flush'.
- `strata:165985b`, `strata:9536143`: pressure gate and regression test.
- Full ctest 23/23; ASan+UBSan ctest 23/23.
- A/A: rows tie; B/A 0.75 to 1.09, CIs include 1.
- A/B: twox1 fillseq 1.80 [1.74, 2.37]; fillrandom 2.25 [1.60, 2.42]; overwrite 1.71 [1.43, 1.81]; fits fillrandom 1.03 [0.96, 1.05] tie.
- Single-trial untested rows are directional only; key-order scan hash was not rerun.
- Fusefilter fence failures on main remain unchased; cause assigned to a separate agent.
- Docs: ledger names `docs/perf/write-p99.md`, but the repository glob found no matching docs files.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
