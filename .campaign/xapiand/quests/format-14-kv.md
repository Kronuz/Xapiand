# format-14-kv: Design format 14 key-value layout

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:4d36aba (2026-10-20); its ledger `docs/status/format-14-kv.md` is the record. Its machine state is in `state.json`: `campaign status format-14-kv`.

## Outcome

The design defines key-ordered value blocks, cache, scans, buffer view, and merges for the core KV layout. Prototype results recommend a split layout and LZ4 default, with zstd 1 as an opt-in merged-output disk preset.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Micro-prototypes, CMake targets, driver, and raw results added: `docs/status/format-14-kv.md`
- A2 [artifact] Design D1-D5, T1-T8, Q1-Q2, proposed K1-K7, and shape summary recorded: `docs/status/format-14-kv.md`
- A3 [artifact] Prototypes build only with -DSTRATA_KV_PROTO=ON; RSS probes support Linux: `docs/status/format-14-kv.md`
- A4 [artifact] Cached prototypes measure both layouts across codecs, calibrated hit rates, and 4-way merges: `docs/status/format-14-kv.md`
- A5 [artifact] Three interleaved-round results and decision tables recorded for Mac and Linux gate VM: `docs/status/format-14-kv.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead, 2026-10-20, Q1: split, contingent on termdict cursor::seek and sequential-decode speed pass; inline is fallback if target is not met.
- Lead, 2026-10-20, Q2: LZ4 default; zstd level 1 on merged output as opt-in disk preset; zstd everywhere not offered.
- Lead, 2026-10-20: T7 disk clause compares against RocksDB's fastest configuration.
- Risk low: research and measurement only (the lead's mapping).

## May land early

None.
