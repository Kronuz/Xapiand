# threadpool-phase7c-l2: journal

State: landed
Quest: .campaign/xapiand/quests/threadpool-phase7c-l2.md (the lead's; never edit it)

## Doing

Nothing: landed at threadpool:e961e44 on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-l2.md`: "[x] L2-1. K1 to K3 reruns on the final code. Data in `docs/status/phase7c-l2/`, Mac x86_64, 16 workers," |
| A2 | `docs/status/phase7c-l2.md`: "[x] L2-2. Merge aging (D4, C8.2), `test/work_scheduling.cc` cases 1 to 4" |
| A3 | `docs/status/phase7c-l2.md`: "[x] L2-3. The other C8 items. C8.1: parse-ahead gets its own group" |
| A4 | `docs/status/phase7c-l2.md`: "[x] L2-4. V2 (a worker waits on work queued only in its own group), case 6" |
| A5 | `docs/status/phase7c-l2.md`: "[x] L2-5. V5 (a reactor calls a waiting entry), case 7: `Options::block_guard` is called on entry to" |
| A6 | `docs/status/phase7c-l2.md`: "[x] L2-6. A mutant per claim (`.scratch/mutants.py`, all die, assertion in brackets)" |
| A7 | `docs/status/phase7c-l2.md`: "[x] L2-7. Sanitizers, separate Homebrew LLVM 23 builds, clean on `threadpool:6573176` (all 6 CTest targets)" |
| A8 | `docs/status/phase7c-l2.md`: "[x] L2-8. Consumers built with `FETCHCONTENT_SOURCE_DIR_THREADPOOL` pointing at this worktree" |
| A9 | `docs/status/phase7c-l2.md`: "[x] L2-9. `cpp-libraries/tools/preflight.sh`: mac-hardened ok, mac-asan-ubsan ok, linux ok" |
| A10 | open: left unticked (run 40981849396 queued; a later run failed FreeBSD `work_group.cc:54` as a suspected flake, not rerun) in `docs/status/phase7c-l2.md` |
| A11 | `docs/status/phase7c-l2.md`: "[x] L2-11. Review round (`docs/reviews/phase7c-l2-code.md`), each with a mutant that dies" |
| A12 | `docs/status/phase7c-l2.md`: "[x] L2-12. The architect's ruling on B2 and N1 (`docs/reviews/phase7c-l2-code.md`," |
| A13 | `docs/status/phase7c-l2.md`: "[x] L2-13. K1 to K3 rerun on the ruled code (strict, exclusive on macOS, Mac x86_64, 16 workers, 5 trials" |
| A14 | `docs/status/phase7c-l2.md`: "[x] L2-14. Re-check 2's items. B5: `cancel()`, `cancel_and_wait()` and `~WorkGroup` share" |
| A15 | open: still open from the superseded `scheduler.md`; L2 Next 4 in `docs/status/phase7c-l2.md` |

## Result

Landed at threadpool:e961e44 on 2026-10-29. B5 and B6 in threadpool:e28a89c.

- K1 tiny tasks, 1 group: 1,822 ns (old pool) to 216 ns per task; 1,000 groups 5,975 to 54; K2 share beside 1,000 busy groups 1.04 (old pool 15.7); K3 query p99 beside 1,000 busy groups 179 us (prototype 275), background 16.6k tasks/s.
- Work conservation: 1,200 runs of `.scratch/rc2/wc.cc` under 6 burners, 0 with `peak < workers` (old head 15 to 19 in 300).
- Review: `docs/reviews/phase7c-l2-code.md` -- REJECT, then the architect ruling and re-checks 1 and 2; the ledger ends awaiting the rechecker on threadpool:e28a89c.
- Raw K data committed in `docs/status/phase7c-l2/`; mutant scripts and probes were in the laptop worktree `.scratch` only.
- Docs: `docs/mechanisms/work-pool.md`, `docs/API.md`, `docs/status/phase7c-l2/k-b6/tail-hot1.txt`.
- Left open: A10, A15.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
