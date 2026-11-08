# cold-tier-step2-item6: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-step2-item6.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:8bb648d on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-step2.md`: "Against `model_total` the catalog's gap is 0.0001 or less in every cell" |
| A2 | `docs/status/cold-tier-step2.md`: "peak resident 110,000, over the cap by 0 in every cell" |
| A3 | `docs/status/cold-tier-step2.md`: "uses met opening 501,756 to 3,014,323, waking 703 to 1,098, demoting 1,637 to 3,647" |
| A4 | `docs/status/cold-tier-step2.md`: "overshoot 10 and 4 slots over 110,000; hit ratio 0.8159, 0.9592" |
| A5 | `docs/status/cold-tier-step2.md`: "ghost ignored: loop30 -0.98 (first 1M -0.84), t099 -0.27 (first 1M -0.59), both killed" |
| A6 | `docs/status/cold-tier-step2.md`: "`use_rows`: open, a handle re-pin and a chore pin (none), demoting waiter" |
| A7 | `docs/status/cold-tier-step2.md`: "`lockstep` (60,000 requests at theta 0.99, 1.2, 0.6, each request equal to `model_total`)" |
| A8 | `docs/status/cold-tier-step2.md`: "thread mutants T-enq, T-unl, T-req, T-pick, T-ghost, T-pass, T-clr all killed under TSan" |
| A9 | `docs/status/cold-tier-step2.md`: "Cost, S8.9 (informational, `quiet strict`, macOS x86_64, one hot key, a stub index" |

## Result

Landed at strata:8bb648d on 2026-11-03. S8 cells match `model_total` within 0.0001; peak resident 110,000 in every cell. Review: `docs/reviews/cold-tier-step2-item6-ghost.md` (APPROVE WITH CONDITIONS); `docs/reviews/cold-tier-step2-item6-code.md` (approve with conditions).

- Merge: Merge branch 'phase/cold-tier-step2-item6' (S3-FIFO with the ghost at half soft_open; C10; CR1).
- Landed as strata:81846d0; item 3 merge strata:a71fad9; item 4 merge strata:ac5ba8e.
- References reproduced: 0.8189 0.8160; 0.8188 0.8153; 0.9593 0.9556; 0.9586 0.9539; 0.5648 0.5535.
- Ghost bytes: 1 hash 152 B; 100 hashes 4,096 B; 1,000 hashes 40,960 B; 55,000 hashes 2,097,152 B.
- Post-ruling policy mutants 35 of 36; all S8 mutants killed; thread mutants 7 of 7.
- TSan and ASan+UBSan policy/stress tests report zero; preflight mac 68/68, Linux 69/69, FreeBSD 68/68.
- Item 5 merge still requires four seam fixes; T4 measurement is next.
- Docs: `docs/design/limits.md`, `docs/mechanisms/catalog.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
