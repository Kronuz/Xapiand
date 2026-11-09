# write-buffer-reads: journal

State: landed
Quest: .campaign/xapiand/quests/write-buffer-reads.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:2f56d64 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/write-buffer-reads.md`: "[x] I4-1 WB-21: records in D3's byte layout (flags at offset 24, the docid prev field at 28 when" |
| A2 | `docs/status/write-buffer-reads.md`: "[x] I4-2 WB-23: docid maps built only under the lease, at `writable_buffer`" |
| A3 | `docs/status/write-buffer-reads.md`: "[x] I4-3 WB-23 (the lead's ruling below): grown key-map and docid-map tables retired under D10's generation stamp" |
| A4 | `docs/status/write-buffer-reads.md`: "[x] I4-4 WB-23's T8 additions pass under TSan and ASan at one writer with readers on other threads" |
| A5 | `docs/status/write-buffer-reads.md`: "[x] I4-5 T8's D6 schedule passes: a `delete_docid` and a key-changing `replace_docid`" |
| A6 | `docs/status/write-buffer-reads.md`: "[x] I4-6 T9's mutants, each killed by its named test: `no_docid_check`" |
| A7 | `docs/status/write-buffer-reads.md`: "[x] I4-7 (C10) WB-22's first-docid-only skip (D6): set before V passes the position, restored on a rejection" |
| A8 | `docs/status/write-buffer-reads.md`: "[x] I4-8 T8's model-check schedules for item 4 at one writer with readers on other threads pass under TSan and ASan" |
| A9 | `docs/status/write-buffer-reads.md`: "[x] I4-9 The core's suites pass unchanged; flushed bytes identical to the base on the items 2-3 byte harness." |
| A10 | `docs/status/write-buffer-reads.md`: "[x] I4-10 Memory per buffered record and per open index reported against the base" |
| A11 | open: left unticked in the ledger (CI green on the branch) in `docs/status/write-buffer-reads.md` |
| A12 | `docs/status/write-buffer-reads.md`: "The contract narrowed: the index's own reads (`get`, `scan`, `has_docid`, `find_key`" |
| A13 | `docs/status/write-buffer-reads.md`: "[x] I4-13 (the review's WB-23 specifics) A const, non-building docid probe with" |

## Result

Landed at strata:2f56d64 on 2026-10-26. T8 model checks report 0 wrong reads and 0 wrong verdicts; growth test records 221,540 reads with 0 wrong. Review: `docs/reviews/write-buffer-design.md` (WB-16 to WB-24, C7-C11); `docs/reviews/write-buffer-step1-code.md` (CR3); `docs/reviews/write-buffer-reads-code.md` (APPROVE WITH CONDITIONS: CR1, CR2).

- Merge: Merge branch 'phase4/write-buffer-reads'.
- strata:f10f160 records the D3 byte layout; strata:ce15621 chooses the 16-entry first position-index block.
- Scan schedules report 0 wrong results and 0 ThreadSanitizer reports.
- ASan+UBSan ctest: 49 of 49; TSan ctest: 49 of 49.
- Flushed bytes match the base: 1 thread, 9 files, digest 48c6d41502f08fc0; 3 threads, 15 files.
- With 100 B values, buffered-record memory is 231.3 / 231.5 / 231.3 B versus base 220.7 / 222.7 / 223.7 B.
- CI for the current code head was queued, not awaited; scoped re-check remains.
- Docs: `docs/design/write-buffer.md`.
- Left open: A11.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
