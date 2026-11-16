# x3-1-chunked-registry: journal

State: landed
Quest: .campaign/xapiand/quests/x3-1-chunked-registry.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:80e8f5a on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/x3-1-chunked-registry.md`: "[x] Revision 4 text (R4.1 to R4.8) and the D4, D5, D9, D10 edits it implies." |
| A2 | `docs/status/x3-1-chunked-registry.md`: "[x] `hazard.hh`: `basic_reader_registry` is a list of chunks (4 records, 320 bytes on 64-byte" |
| A3 | `docs/status/x3-1-chunked-registry.md`: "[x] Litmus (`hazard_litmus_test`) reruns on the chunked registry (K = 1 so every race crosses" |
| A4 | `docs/status/x3-1-chunked-registry.md`: "[x] Tests (`hazard_test`): `chunks` (exact capacity, allocation per chunk, stable record" |
| A5 | `docs/status/x3-1-chunked-registry.md`: "[x] Sanitizers: `hazard_test`, `hazard_litmus_test`, `shared_view_test`, `shared_read_test`" |
| A6 | `docs/status/x3-1-chunked-registry.md`: "[x] Mutants, `tools/mutants/chunked.py` (6 of 6 killed, unmutated survives):" |
| A7 | `docs/status/x3-1-chunked-registry.md`: "[x] Bench, `bench/shared_reader/registry.cc` (`shared_reader_registry`), under `quiet strict`" |
| A8 | `docs/status/x3-1-chunked-registry.md`: "[x] `cpp-libraries/tools/preflight.sh . mac` at `12a9d89`: `mac-hardened: ok` and `mac-asan-ubsan: ok`" |
| A9 | `docs/status/x3-1-chunked-registry.md`: "[x] Architect check (`docs/reviews/x3-1-chunked-registry.md`), land with conditions:" |

## Result

Landed at strata:80e8f5a on 2026-11-02. `hazard_litmus_test`: 10^7 iterations, 0 free-while-read at seq_cst. Review: `docs/reviews/x3-1-chunked-registry.md` (land with conditions).

- Merge: Merge branch 'phase7c/x3-1-chunked-registry' (per-index chunked reader registry, shared-reader revision 4).
- Registry chunks are 4 records and 320 bytes on 64-byte lines; registry object is 120 bytes on macOS.
- Benchmark: acquire costs about 1 ns more; scans scale with claimed records, not capacity.
- Six binaries pass under ASan+UBSan and TSan; 7 of 7 mutants killed after review.
- mac hardened and ASan+UBSan preflight each pass 60 of 60 tests.
- Linux, FreeBSD, arm64, and CI results remain open.
- Docs: `docs/design/shared-reader.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
