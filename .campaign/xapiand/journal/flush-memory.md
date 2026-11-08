# flush-memory: journal

State: landed
Quest: .campaign/xapiand/quests/flush-memory.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:5685428 on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/flush-memory.md`: "2331446: reserve, borrow (core/borrow.hh: borrowable_vector, lease; the buffer's type enforces the rule)" |
| A2 | `docs/status/flush-memory.md`: "fuzz_stored differential commit: borrowing writer equals copying writer." |
| A3 | `docs/status/flush-memory.md`: "flush_memory_test pins one-run and three-run flush digests taken from main" |
| A4 | `docs/status/flush-memory.md`: "Review fixes: CR1 flush_threads comment states run threads call File::write_at" |

## Result

Landed at strata:5685428 on 2026-10-21. Release ctest 25/25; ASan+UBSan 25/25; TSan 25/25, no reports; fuzz_stored 25,288 runs clean. Review: `docs/reviews/flush-memory.md` -- approve with conditions.

- Merge: Merge remote-tracking branch 'origin/phase4/flush-memory'.
- `strata:2331446`: reserve, borrow, streaming, and `flush_memory_test`; test commit verifies no copy at first write.
- `strata:2331446` and subsequent fuzz commit: differential check against copying writer.
- Format 14 fixtures pass; digests 0x9ce03837c27a5408 and 0x07bef1f86cb043c8.
- Peak RSS median 95.5 -> 70.7 MiB (-25.9%), A/A 1.2%, six interleaved trials.
- Known gap: no test observes `flush_documents` calling reserve; its builder effect is tested.
- Not merged; next step is merge to main.
- Docs: ledger names `docs/perf/peak-memory.md`, but the repository glob found no matching docs files.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
