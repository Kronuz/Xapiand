# phase1: journal

State: landed
Quest: .campaign/xapiand/quests/phase1.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:6a3cddde on 2026-10-11 under the earlier campaign skill.

## Next

1. Nothing: landed. What stays open is named under Result.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/gates/phase-1.md`: "`bitpack::decode` and `decode_delta` take the input size and return 0 for malformed or truncated blocks" (bitpack rows, Robustness) and `docs/reviews/phase-1.md` (re-check: APPROVED) |
| A2 | `docs/gates/phase-1.md`: "`fst::map` validates its bytes at open: every field in bounds" (fst rows; open-time validation) |
| A3 | `docs/gates/phase-1.md` (docset row); `docs/reviews/phase-1.md`: "They match, with four accepted relocations and three additions." |
| A4 | `docs/gates/phase-1.md` (task rows); `docs/reviews/phase-1.md`: "They match, with four accepted relocations and three additions." |
| A5 | `docs/gates/phase-1.md`: "`file`, `pinned` (CRC-32C verified), `backend` concept, `sync_backend`" (residency: file, pinned, backend concept, sync_backend; strata::scorer) |
| A6 | `docs/gates/phase-1.md` (hardware CRC-32C in hashes, 14 to 19 GB/s, about 40x the journal's byte loop); its own library Kronuz/crc32c since strata:28dbdea5 (2026-10-20) |
| A7 | `docs/gates/phase-1.md`: "bytes identical to SIMD-BP128 at all 32 widths" (bytes identical to SIMD-BP128 at all 32 widths; golden fixture of 99 FOR and PFOR blocks) |
| A8 | open: not in the gate report or the review; fst's own test/test.cc checks prefix automata against std::map on random sets |
| A9 | `docs/gates/phase-1.md`: "All pass; 10^8 in a release build, 10^7 under ASan/UBSan and TSan (macOS)" (all pass; 10^8 in a release build, 10^7 under ASan, UBSan and TSan on macOS, 10^6 under GCC and Clang sanitizers on Linux, AArch64 under qemu) |
| A10 | `docs/gates/phase-1.md`: "4.5% of entries. LEB128 1.85 B/value, 2.60 ns" (tails 4.5% of entries; LEB128 1.85 B/value at 2.60 ns added to varint) |
| A11 | open: decode passes (0.97x SIMD-BP128, 1.00x with delta; 2.2 to 6x scalar); the size row is an estimate at the gate (958.7 MB against Lucene's .doc 990.6 MB with skip data); Phase 2's postings row settles it (1.017x .doc, quest phase2 A12) |
| A12 | open: `docs/gates/phase-1.md`: 10% of the hash map's bytes (5.91 B/key), but full-vocabulary lookup 2.43x misses the 2x target; the block-index use passes at 1.63x, which the review accepted |
| A13 | `docs/gates/phase-1.md`: "Worst rows at density 0.01: OR 1.15×, ANDNOT 1.07–1.17× (Linux VM)" (worst rows at density 0.01: OR 1.15x, ANDNOT 1.07 to 1.17x; Pass, provisional) |
| A14 | `docs/gates/phase-1.md`: "13.9–21.5 ns (Linux, GCC/Clang), 17.9 ns (macOS), 0 allocations" (13.9 to 21.5 ns Linux, 17.9 ns macOS, 0 allocations; Asio p99 175 ns) |
| A15 | `docs/gates/phase-1.md`: "libFuzzer with ASan and UBSan, one CPU-hour each in the gate VM, zero findings" (Robustness: bitpack 1,742,985, fst 285,749, varint 1,980,854 fuzz runs, zero findings; golden fixtures; pins) |

## Result

Landed at strata:6a3cddde on 2026-10-11. Gate report `docs/gates/phase-1.md` (strata:1e74dec6, robustness strata:dfd61120); review `docs/reviews/phase-1.md`: not approved on B1 to B4, then the re-check: "Phase 1 gate: APPROVED".

- bitpack: 0.97x SIMD-BP128 decode, 1.00x with delta (Linux VM), 0.99x and 1.02x (macOS); bytes identical to SIMD-BP128 at all 32 widths.
- fst: 5.91 bytes per key, 10% of the hash map; full-vocabulary lookup 2.43x (memory-latency bound, a 23 MB FST past the LLC); floor_value over a 1/32 block index 1.63x.
- docset: at most 1.15x CRoaring (Linux VM), 1.14x (macOS), most rows below 1.
- task: no-op task 13.9 to 21.5 ns with 0 allocations; Asio yield-to-resume p99 175 ns.
- Findings recorded: symmetric transfer is not portable (GCC 13 tail-calls only at -O2 without sanitizers; task uses a per-thread trampoline); POPCNT is not implied by SSE4.2 target attributes on Clang; Xapiand's length codec is MIT by Olly Betts' permission (Xapiand:1691e5b2e).
- Docs: `docs/gates/phase-1.md`, `docs/reviews/phase-1.md`, and each library's README and ARCHITECTURE.
- Left open: A8, A11, A12.

## Rulings I made

None beyond the quest file's Decisions (the plan and the reviews are the record).
