# phase7c-l5: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-l5.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:c82ac9f on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-l5.md`: "[x] K1 `defer_io` option; `apply` makes no device call on each owed path, counted with faultfs `calls()` (new)" |
| A2 | `docs/status/phase7c-l5.md`: "[x] K2 The owed verdicts and their chores (`refresh`, `reclaim_for_space`, `check_in`), then the retry accepted" |
| A3 | `docs/status/phase7c-l5.md`: "[x] K3 C15: oversize (document cap, lane cap) ends: invalid on an empty buffer" |
| A4 | `docs/status/phase7c-l5.md`: "[x] K4 Mutants (`.scratch/mut/run.py`, results in `.scratch/mut/results.txt`): 12 of 12 killed" |
| A5 | `docs/status/phase7c-l5.md`: "[x] K5 `preflight.sh` mac (hardened; ASan+UBSan), Linux and FreeBSD VMs all ok on 27c944e (61 tests each)" |
| A6 | `docs/status/phase7c-l5.md`: "[x] K6 `default_path_unchanged` (golden values captured on main 8fac1bc: verdicts, 163 mutating calls" |
| A7 | `docs/status/phase7c-l5.md`: "[x] K7 `work_pool_executor` through the executor concept" |
| A8 | `docs/status/phase7c-l5.md`: "[x] K8 `docs/mechanisms/defer-io.md`." |

## Result

Landed at strata:c82ac9f on 2026-10-29. ctest: 61 of 61 on the reported 27c944e verification; later ctest: 60 of 60 after pool retirement. Review: Xapiand `docs/reviews/phase7c-design.md` (R5, C15, C16; design review); Xapiand `docs/reviews/phase7c-x2b-code.md` (architect ruling on reader registry, X3-1).

- Merge: Merge branch 'phase7c/l5-defer-io' (defer_io host mode and the WorkPool executor).
- strata:4563ed1 delivers the option, verdicts, binding, tests, and mechanism note; strata:0bc8969 and strata:27c944e add fixes and review conditions.
- Mutants: 12 of 12 killed; a later group-wait mutant is killed.
- mac, Linux, and FreeBSD preflights pass; listed TSan subset is 10 of 10.
- Full CI was pushed, results not watched; code review and architect scoped check remain.
- Docs: `docs/mechanisms/defer-io.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
