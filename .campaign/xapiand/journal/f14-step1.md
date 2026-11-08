# f14-step1: journal

State: landed
Quest: .campaign/xapiand/quests/f14-step1.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:75d1e7e on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/f14-step1.md`: "[x] K1 (T1): fixtures re-pinned at the new pre-freeze version; format-13 segments refused as" |
| A2 | `docs/status/f14-step1.md`: "[x] K2 (T3): `check()` refuses a segment out of key order; a docid-order mutant fails the test." |
| A3 | `docs/status/f14-step1.md`: "[x] K3: a merged document-layout segment is key-ordered with every live record once, its digest" |
| A4 | `docs/status/f14-step1.md`: "[x] K4: gets allocate nothing per call besides the returned string, and nothing with" |
| A5 | `docs/status/f14-step1.md`: "[x] K5 (T2): the block decoder's fuzz target and corpus, replayed in CI; ASan and UBSan clean;" |
| A6 | `docs/status/f14-step1.md`: "[x] K6: CI green on Linux, macOS, and FreeBSD (`cpp-libraries/tools/preflight.sh` first);" |
| A7 | `docs/status/f14-step1.md`: "[x] K7: a branch A/B with the KV harness (`kv_strata` on the core API) against main at the quick" |
| A8 | `docs/status/f14-step1.md`: "[x] Prospect at the branch's strata commit: its search layout (64 KiB blocks) passes its suite." |

## Result

Landed at strata:75d1e7e on 2026-10-20. K3 memory charge grows 0.00 B per added key byte with streaming, versus 2.00 B with key-map mutant. Reviews: `docs/reviews/f14-step1-decoder.md` -- APPROVE WITH CONDITIONS; its SR2 and SR4 fixes: `docs/reviews/sr2-sr4.md`.

- Merge: Merge remote-tracking branch 'origin/phase4/f14-step1'.
- strata:1ce0ccb implements the format; strata:ec8df01 adds format14_test and get_alloc_test.
- strata:ba2fa9c adds fuzz_stored and its corpus (249 inputs, 844 KB).
- strata:59cdca3 merges main 6e13416; strata:42ebbb4 is the recorded tip.
- compressors:9d2a87c supplies pinned block codecs; strata:4c5df8b repins compressors.
- K4: 50,000 gets; get(key) allocates 40,149 times for 40,149 hits; get(key, f) allocates 0.
- K5 fuzz: 367,541 executions, 0 findings; ASan+UBSan ctest 23/23 on macOS.
- K6 strata CI run 39522635907 green on all eight jobs; compressors CI green on all eight jobs; Detent 168/168; K7: readrandom 43.2× faster; YCSB A 9.15×, B 23.4×, C 29.5×, D 13.1×, F 15.2×; overwrite 1.12× slower.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
