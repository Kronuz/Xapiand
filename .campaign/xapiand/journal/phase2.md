# phase2: journal

State: landed
Quest: .campaign/xapiand/quests/phase2.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:6932b812 on 2026-10-13 under the earlier campaign skill.

## Next

1. Nothing: landed. What stays open is named under Result.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/gates/phase-2.md`: "`test/fixtures/small.sx` pins the writer's bytes at format 11" ("Changes for the architect": format versions 2 to 11; golden fixture small.sx at format 11) |
| A2 | `docs/gates/phase-2.md`: "419 of 419 queries equal in counts and top-10, counted and pruned" (Results; NEAR included in parity) |
| A3 | `docs/gates/phase-2.md`: "windowed MaxScore for disjunctions; lead-block windows for conjunctions; counting from statistics where known" ("Evaluation": windowed MaxScore, lead-block windows, counting from statistics, the 2k-buffer top-k collector) |
| A4 | `docs/gates/phase-2.md`: "`termdict` 60 CPU-minutes (4.2M runs, clean) for its verifier hook" (termdict lookup row; term dictionary breakdown under Open) |
| A5 | `docs/gates/phase-2.md`: "966 vectors and two per-code-point sweeps over all 1,114,112 code points" (Analyzer: 966 vectors and two sweeps over all 1,114,112 code points) |
| A6 | `docs/gates/phase-2.md`: "positions, and stored records are verified per 64 KiB region on first read" (Integrity: 64 KiB regions verified on first read; max_score(from, upto) contract) |
| A7 | `docs/gates/phase-2.md`: "strata wins every class of every command (0.006–0.34× Xapian's time, floor-subtracted) and indexes 8.9× faster" (0.006 to 0.34x Xapian's time on every class; indexing 8.9x faster) |
| A8 | `docs/gates/phase-2.md`: "Five trials: every class of every command within 1.5×, the 95% interval's upper bound included." (five trials, every class within 1.5x including the interval's upper bound; worst TOP_1000 intersection 1.45x [1.44 to 1.47]) |
| A9 | `docs/gates/phase-2.md`: "Counts equal Tantivy's on all 962 + 200 queries in every trial" (counted classes within 1.5x; counts equal Tantivy's on 962 + 200 queries) |
| A10 | `docs/gates/phase-2.md`: "67 rare (df < 1,000), 67 medium, 66 frequent (df ≥ 100,000), seed 2026" (67 rare, 67 medium, 66 frequent terms, seed 2026) |
| A11 | `docs/gates/phase-2.md`: "Three trials each, median: 9,660 docs/s, 1.53× Lucene (6,290) and 0.84× Tantivy (11,550)" (9,660 docs/s: 1.53x Lucene, 0.84x Tantivy, three trials) |
| A12 | `docs/gates/phase-2.md`: "Format 11: 1,007.9 MB against 990.6 MB, 1.017× (format 10 was 1.026×)" (format 11: 1,007.9 MB against 990.6 MB, 1.017x) |
| A13 | `docs/gates/phase-2.md`: "Medians of batches of 1,024, interleaved runs: 0.65–0.70×" (0.65 to 0.70x seekExact by batch medians; the single-lookup p99 is owed on bare metal) |
| A14 | `docs/gates/phase-2.md`: "and `NEAR` included; worst relative error 3.6e-16" (419 of 419 queries equal in counts and top-10; worst relative error 3.6e-16) |
| A15 | `docs/gates/phase-2.md`: "One CPU-hour per target at `63a5339`, in parallel, ASan and UBSan, a 10 s hang timeout" (Robustness: one CPU-hour per target at strata 63a5339, zero findings; ASan, UBSan, TSan) and `docs/reviews/phase-2-gate.md` (G3 closes) |
| A16 | open: serving memory recorded (`docs/gates/phase-2.md`, "Memory and disk": strata 750 MB peak, 1.33x Tantivy's resident index pages); heap bytes and allocations per query carried as condition C15 of `docs/reviews/phase-2-gate.md` |
| A17 | `docs/gates/phase-2.md`: "5M index on disk: strata 2.77 GB, Lucene 2.81 GB (0.99×), Tantivy 3.03 GB, Xapian" (5M index: strata 2.77 GB, Lucene 2.81 GB, Tantivy 3.03 GB, Xapian 15.4 GB; per section against Lucene) |

## Result

Landed at strata:6932b812 on 2026-10-13. Gate report `docs/gates/phase-2.md` at strata 63a5339, format 11, on the Linux gate VM (in-VM fsync p50 2.9 ms, p99 4.4 ms). Review `docs/reviews/phase-2-gate.md`: REJECT (G1 to G5), second re-check not approved, third re-check "APPROVED WITH CONDITIONS".

- Queries: every class of every command within 1.5x of the faster of Lucene and Tantivy (worst 1.45x floor-subtracted); strata beats Xapian on every class (0.006 to 0.34x its time).
- Indexing, one thread, 1M documents: 9,660 docs/s, 1.53x Lucene, 0.84x Tantivy.
- Disk: postings 1.017x Lucene's .doc; the term dictionary 2.53x (101.6 against 40.2 MB), the target of P12 and of Phase 3's disk ratchet.
- Parity: 419 of 419 oracle queries equal; verify on 1,162 queries at k = 10, 100, 1,000 with 0 mismatches.
- Fuzz, one CPU-hour each, zero findings: fuzz_segment 64,634 runs, snowball 76,670,052, analysis 22,802,463, bitpack 324,447,843, unicode 523,661,330.
- Carried to Phase 3: conditions C1 to C3, C5, C7, C9, C13 to C16 (quest `phase3-ratchets` A10); the termdict single-lookup p99 on bare metal.
- Docs: prospect `docs/gates/phase-2.md`, `docs/reviews/phase-2-gate.md`; strata `docs/design/phase-2.md`.
- Left open: A16.

## Rulings I made

None beyond the quest file's Decisions (the plan and the reviews are the record).
