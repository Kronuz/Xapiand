# phase3-lifecycle: journal

State: landed
Quest: .campaign/xapiand/quests/phase3-lifecycle.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:950d24d on 2026-10-17 under the earlier campaign skill.

## Next

1. Nothing: landed. The ratchet rows are quest `phase3-ratchets`.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/gates/phase-3-lifecycle.md`: "The merge policy (`405fbc0`) meets Lucene 10.4.0's TieredMergePolicy contract" (T1 and T2 workloads: refreshes, budget flushes, merges, commits; background merges on the host's executor) |
| A2 | `docs/gates/phase-3-lifecycle.md`: "over adds, replaces, and deletes by key and docid, rejected transactions, metadata, refreshes, budget flushes" (T1: adds, replaces, deletes by key and docid, rejected transactions, metadata, budget flushes) |
| A3 | `docs/gates/phase-3-lifecycle.md`: "replay of a whole log after a commit-and-reopen and after `install`, the applied half skipped" (T2: 40 seeded workloads under three schedules; replay after commit-and-reopen and after install equal to the uninterrupted run) |
| A4 | `docs/gates/phase-3-lifecycle.md`: "`faultfs` models a crash per sector (old, new, or zeros for each unsynced sector)" ("Power loss and digests": faultfs models a crash per sector and per namespace operation) |
| A5 | `docs/gates/phase-3-lifecycle.md`: "`small.sx` (format 12), `manifest.bin`, `live.bin`, `meta.bin`; termdict's `small.td`" (T6 Fixtures: small.sx at format 12, manifest.bin, live.bin, meta.bin; a test rejects format 11) |
| A6 | `docs/gates/phase-3-lifecycle.md`: "`lifecycle_test`: 10,000 crash points over 31 seeded workloads applied at log positions" (T1: 10,000 crash points over 31 seeded workloads, 555 inside recoveries, zero losses, zero inconsistent opens; background merges at 10,000 points, strata:e36a793c) |
| A7 | `docs/gates/phase-3-lifecycle.md`: "the digest compared after every transaction on the first ten seeds (D12)" (T2 Pass) |
| A8 | `docs/gates/phase-3-lifecycle.md`: "one-document commits run until admission answers `no_space` (47 of them) and never hit ENOSPC" (T5 Pass: admission answers no_space before ENOSPC; ENOSPC mid-file recovers in place; a failed sync fences) |
| A9 | `docs/gates/phase-3-lifecycle.md`: "every test (16) under ASan with UBSan (`-fno-sanitize-recover=undefined`), T1 at 10,000 crash points" (T6 Fuzzing, Sanitizers and Coverage C16: Pass) and `docs/reviews/phase-3-lifecycle.md` (fourth re-check: APPROVE WITH CONDITIONS) |
| A10 | open: `docs/gates/phase-3-lifecycle.md` (T6 Corruption checks: every check reached but five guards, open with their reasons) |
| A11 | `docs/design/reordering.md` (T1 to T7) and `docs/gates/phase-3-ratchets.md`: "Disk with P1: whole index 0.999× keyed `-bp` (pass)" ("Reordering's effect on queries": 0.70 to 1.01x of the docid-order index; disk with P1 0.999x keyed -bp) |
| A12 | `docs/perf/p6-multisegment.md` (merged-index results R6; one collector across segments); `docs/gates/phase-3-ratchets.md`: "merging and P6 bring strata's global excess from 1.58× to 1.33× at `TOP_10`" |
| A13 | `docs/perf/p12-term-dictionary.md`: "df packed with the flags; a single posting's df, cf, and wdf of 1 implied" (101.6 to 46.3 MB, 2.53x to 1.15x Lucene) |
| A14 | `docs/perf/p15-region-size.md`: "The default moves from 64 KiB to 4 KiB." (the default region from 64 KiB to 4 KiB) |

## Result

Lifecycle rows passed at strata:2a111373 on 2026-10-14; recorded at strata:950d24d on 2026-10-17 with the Track P items committed to Phase 3 that landed. Gate report `docs/gates/phase-3-lifecycle.md` (at strata 85b3cdc for the fourth re-check); review `docs/reviews/phase-3-lifecycle.md`: REJECT, three further re-checks, then "APPROVE WITH CONDITIONS: the lifecycle rows pass".

- T1 power loss: 10,000 crash points over 31 workloads, sector-granular, 555 points inside recoveries, install at every mutation, create at every mutation under 50 seeds; reclaim across 3,635 points; zero losses, zero inconsistent opens. Background merges: 10,000 points at strata e8accae (condition 7).
- T2 digest: 40 workloads under three schedules, equal digests and verdicts.
- T5 disk full: admission refuses before the disk does; on a 64 KiB disk, 47 one-document commits then no_space, never ENOSPC.
- Media faults: EIO or one flipped bit at 782 offsets; transient faults heal, sticky ones fail closed; the digest never differs.
- T6: fuzz_files 74,383,545 runs, fuzz_segment 156,376 runs plus 1.08 CPU-hours on the M1, termdict 12,863,232 runs, zero findings; coverage at every file's targets.
- P1: reordering takes about 20% off query times (0.70 to 1.01x of docid order) and the whole index to 0.999x keyed Lucene -bp; the 5M reordering merge 846 s on 8 threads (6.6x slower than a docid merge at first).
- P12: the term dictionary from 101.6 to 46.3 MB (2.53x to 1.15x Lucene's .tim, .tip and .tmd); the rest is carried to format 13 (`phase3-ratchets` A5).
- Docs: `docs/gates/phase-3-lifecycle.md`, `docs/reviews/phase-3-lifecycle.md`, `docs/design/phase-3.md`, `docs/design/reordering.md`; prospect `docs/gates/phase-3-ratchets.md`, `docs/perf/p6-multisegment.md`, `docs/perf/p12-term-dictionary.md`, `docs/perf/p15-region-size.md`.
- Left open: A10.

## Rulings I made

None beyond the quest file's Decisions (the plan and the reviews are the record).
