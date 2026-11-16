# f14-step1: Key-ordered format 14 segments

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:75d1e7e (2026-10-20); its ledger `docs/status/f14-step1.md` is the record. Its machine state is in `state.json`: `campaign status f14-step1`.

## Outcome

Format 14 writes key-ordered document segments in codec-tagged stored blocks, with key-ordered merges and allocation-conscious gets. The format supports LZ4 by default and an opt-in zstd 1 merged-output preset.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 (T1): fixtures re-pinned; format-13 segments refused as unsupported_segment; core_kv_test digest identical at main and branch: `docs/status/f14-step1.md`
- A2 [artifact] K2 (T3): check() refuses segments out of key order; docid-order mutant fails: `docs/status/f14-step1.md`
- A3 [artifact] K3: merged document-layout segment is key-ordered, contains each live record once, and preserves input digest; ascending path, no records scratch file or key map: `docs/status/f14-step1.md`
- A4 [artifact] K4: gets allocate only the returned string; get(key, f) allocates nothing: `docs/status/f14-step1.md`
- A5 [artifact] K5 (T2): decoder fuzz target and corpus replayed in CI; ASan and UBSan clean; fix-revert tests fail: `docs/status/f14-step1.md`
- A6 [artifact] K6: CI green on Linux, macOS, and FreeBSD; compressors additive and CI green; Detent suite replayed: `docs/status/f14-step1.md`
- A7 [artifact] K7: quick-tier branch A/B against main, with A/A first, under quiet holds; every row reported: `docs/status/f14-step1.md`
- A8 [artifact] Prospect search layout at branch strata commit passes its suite: `docs/status/f14-step1.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead's format decisions: split layout; LZ4 default; zstd 1 on merged output as opt-in per-index disk preset; T7 disk clause against RocksDB.
- Developer decision: get buffer is one per thread, shared across indexes, with nested callbacks taking another buffer.
- Developer decision: merges stream keys into dictionary and walk source cursors twice.
- Developer decision: zstd_merged applies to any layout's merged output and is off by default.
- Developer decision: key-ordered merge refuses caller-supplied ordering.
- Risk high: freezes a persistent segment format (the lead's mapping).

## May land early

None.
