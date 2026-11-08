# storage-phase7c-l6: Streamed record write (L6)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at storage:c3d3efb (2026-10-30); its ledger `docs/status/phase7c-l6.md` in Kronuz/storage is the record. The quest's repository is `../storage`. Its machine state is in `state.json`: `campaign status storage-phase7c-l6`.

## Outcome

storage gains `Storage::StreamWriter` (begin with a known size, append chunks, commit or abandon) and `reclaim_volumes` at open, byte-compatible with `write()`, crash-safe at every chunk boundary, with 64-bit offsets fixed in `read()`, `write()` and `growfile()`.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Byte-equal read-back around block, chunk, 16 MiB cap and 2x cap (`test_roundtrips`): `docs/status/phase7c-l6.md`
- A2 [artifact] Same bytes as `write()`; existing readers unchanged (`test_compat`): `docs/status/phase7c-l6.md`
- A3 [artifact] A 4 GiB - 1 record reads back; 4 GiB + 1 refused (`storage_stream_big`, `test_limits_and_misuse`): `docs/status/phase7c-l6.md`
- A4 [artifact] A record starting past 4 GiB of byte offset reads: `docs/status/phase7c-l6.md`
- A5 [artifact] Nothing visible before commit; crash at every chunk boundary and write (`test_crash_sweep`): `docs/status/phase7c-l6.md`
- A6 [artifact] An abandoned writer leaves nothing (`test_abandon`): `docs/status/phase7c-l6.md`
- A7 [artifact] Orphans reclaimed at open (`test_reclaim`); volume-full refusal at begin (`storage_stream_full`): `docs/status/phase7c-l6.md`
- A8 [artifact] 21 of 21 mutants killed; three fixes found on the way each with a failing-when-reverted test: `docs/status/phase7c-l6.md`
- A9 [artifact] ASan+UBSan whole suite clean; libFuzzer 200k runs clean; CI run 41035377548 green on 8 jobs: `docs/status/phase7c-l6.md`
- A10 [artifact] Review fixes L6-B1, L6-N1, L6-N3, L6-N4 each with a test that fails when reverted; L6-N2 documented: `docs/status/phase7c-l6.md`
- A11 [artifact] L6-N5 (cursor assert under NDEBUG, error text): `docs/status/phase7c-l6.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner ruling: blobs stay minimal and format-neutral (design `docs/design/phase7c.md` D9, section 5 L6; R9/F19).
- The follow-up commit on `phase7c/l6-streamed-write` (pin compressors at the LZ4 bounds fix, fuzz the flags byte) landed as storage:e4c5746; the branch tip storage:4bf7ef4 is its unmerged twin.
- Risk high: bytes on disk and crash consistency of streamed records (the auditor's mapping).

## May land early

None.
