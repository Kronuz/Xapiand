# cold-tier-step2-item5: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-step2-item5.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:676285d on 2026-11-04 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-step2.md`: "SW `demote_window` (a pin lands between the load and the exchange)" |
| A2 | `docs/status/cold-tier-step2.md`: "SW `close_from_warm_only` (hot slots are demoted, only warm ones close, no close is refused)" |
| A3 | `docs/status/cold-tier-step2.md`: "SW `queued_reclaim` (deletes inline and posted: `reclaim()` posted once" |
| A4 | `docs/status/cold-tier-step2.md`: "SW `reclaim_zero_under_demoting` (the chore sees `demoting`" |
| A5 | `docs/status/cold-tier-step2.md`: "SW `stuck_close` (reported once, counted, a waiter suspended, two retries fail" |
| A6 | `docs/status/cold-tier-step2.md`: "SW `coalesced_pass` (100 posts, one pass), `repost_while_over` (7 slots, cap 2, 2 posts a pass" |
| A7 | `docs/status/cold-tier-step2.md`: "SW `shutdown_commits_and_closes` (six slots: committed, commit fails, clean, negative entry, pinned" |
| A8 | `docs/status/cold-tier-step2.md`: "SW `shutdown_commits_and_closes` (`payload::destroyed_in_close == 0`)" |
| A9 | `docs/status/cold-tier-step2.md`: "SW `destroyed_pinned_asserts` (a forked child aborts; with the assert off, two breaks)" |
| A10 | `docs/status/cold-tier-step2.md`: "SW `destroyed_open_leaks` (one break, the budget still charged, the directory lock still held)" |
| A11 | `docs/status/cold-tier-step2.md`: "SW `reuse_is_clean` (three failed opens, erased, another key on the slot fails once: 10 ms, not 40" |
| A12 | `docs/status/cold-tier-step2.md`: "the third succeeds after the guard goes and the waiter reopens in place on `gen` + 1" |
| A13 | `docs/status/cold-tier-step2.md`: "SW `close_throws` (hook `close.call`: the slot ends at `open`, counted, closes next time)" |
| A14 | `docs/status/cold-tier-step2.md`: "SW `negative_entry_erased` (9.99 s no, 10 s yes; edge counted; a retry arrives first and wins)" |

## Result

Landed at strata:676285d on 2026-11-04. Release: `catalog_test` 300 seeds, 48,000 steps; 3,984 passes, 575 demotes, 840 closes, 1,673 negative entries erased, 541 shutdown commits. Review: `docs/reviews/cold-tier-step2-item5.md` (REJECT on B1-B4, C21-C24; fix round recorded).

- Merge: Merge branch 'phase/cold-tier-step2-item5' (cold-tier step 2 item 5).
- Sweep and shutdown implemented in `catalog.hh`; item 6's S3-FIFO policy was merged with seam conditions.
- Stress: 3,000 ops; 2,665 passes and 7,997 closes; shutdown overlaps 3,000+ pins.
- Mutants Z1-Z15 and Z17-Z42 plus T1 and T3 die; T2 survives under TSan.
- Final preflight: mac-hardened 68/68, mac-asan-ubsan 68/68, FreeBSD 68/68; Linux not run.
- CI and architect re-check remain with the lead.
- Docs: `docs/mechanisms/catalog.md`, `docs/design/limits.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
