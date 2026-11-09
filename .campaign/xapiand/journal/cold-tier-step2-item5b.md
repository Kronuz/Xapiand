# cold-tier-step2-item5b: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-step2-item5b.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:5d742b2 on 2026-11-04 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-step2.md`: "`create_passes_absent`: ten reads fail `index_not_created` (nine of them without an open)" |
| A2 | open: destroying H, then the reader, index and IO in that order (F6) is recorded "not done (not cheap)"; only H-first is checked, and mutants F6a and F6b survive (`docs/status/cold-tier-step2.md`) |
| A3 | `docs/status/cold-tier-step2.md`: "`reopen_drains_holders`: three holders, a fence, `reopen`; by-key and by-handle pins wait" |
| A4 | `docs/status/cold-tier-step2.md`: "`fenced_idle_is_replaced`, `fenced_owed_wake_replaces`, `fenced_wake_counts_holders`" |
| A5 | `docs/status/cold-tier-step2.md`: "`check_in_before_demote`: a cascade of background merges on the inline executor" |
| A6 | `docs/status/cold-tier-step2.md`: "`snapshot_and_flush`: five slots (open, owed, failed, open, closed after the snapshot)" |
| A7 | `docs/status/cold-tier-step2.md`: "`host_files_counted`: two files per open reserved on `opening.counts`" |
| A8 | `docs/status/cold-tier-step2.md`: "the oracle (a wake posted inside a backoff aborts) over `catalog_test 1000` and `catalog_stress_test 8000 6`: clean" |
| A9 | open: revision 8 text written in the ledger, not applied to the design in `docs/status/cold-tier-step2.md` |
| A10 | open: the by-key smoke A/B reads 3% above item 5, "not proved either way here"; the lead's measurement on main decides it (`docs/status/cold-tier-step2.md`) |
| A11 | `docs/status/cold-tier-step2.md`: "`cancel_pin` runs `cancel_wait` only while `w->mu` is set; `abandon` is static and tests `w->mu`" |
| A12 | `docs/status/cold-tier-step2.md`: "a frame dies only after its scheduler is closed (task's phase 2), or while no delivery can run" |
| A13 | `docs/status/cold-tier-step2.md`: "`stats().cancels` counts the waits a token ended; a pin whose token was requested before it waited does not count" |
| A14 | `docs/status/cold-tier-step2.md`: "PF5: a requested token still starts the open or the wake (`lock(token)` has no side effect" |

## Result

Landed at strata:5d742b2 on 2026-11-04. Waiter teardown tests: 200 seeds x 80 steps; rel 164 frames destroyed waiting, 159 unlinked, 5 gone pins released. Review: `docs/reviews/cold-tier-step2-item5b.md` (APPROVE WITH CONDITIONS); `docs/reviews/cold-tier-step2-item5b-pins-code.md` (APPROVE WITH CONDITIONS).

- Merge: Merge branch 'phase/cold-tier-step2-item5b' (catalog host interface and destroy-safe pins).
- Host interface at strata:3034c32 and follow-ups; destroy-safe pins strata:f052782, strata:177a687, strata:edb880b; merged with item 5 at strata:4319bb9, main at strata:7f12633.
- ASan+UBSan 828 / 801 / 27; TSan 983 / 947 / 36.
- Mac full ctest rel 72/72 and ASan+UBSan 72/72; Linux full ctest 71/71; Linux TSan and ASan+UBSan targeted suites report zero findings.
- C26: the composed slot fits at 184 B libc++ and 192 B libstdc++ (circular `wait_list`).
- CI run 41841864067 green across FreeBSD, Linux, clang/gcc, x86_64 and arm64; CI on strata:cee8079 green.
- PF1 to PF5 closed in the ledger; the lead's confirmation of PF1 was still owed.
- Docs: `docs/mechanisms/catalog.md`.
- Left open: A2, A9, A10.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
