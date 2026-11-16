# format-14-kv: journal

State: landed
Quest: .campaign/xapiand/quests/format-14-kv.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:4d36aba on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/format-14-kv.md`: "`6abf075`: the four micro-prototypes (`bench/kv/proto/`: blocks, cache, buffer, bufmem)" |
| A2 | `docs/status/format-14-kv.md`: "`bb78b18`: the design (D1 to D5, T1 to T8, Q1 and Q2, step 1 with proposed done criteria" |
| A3 | `docs/status/format-14-kv.md`: "`8c33162`: the prototypes build only with `-DSTRATA_KV_PROTO=ON`; their RSS probes read Linux" |
| A4 | `docs/status/format-14-kv.md`: "`785bf19`: Q1 and Q2's prototype `kv_proto_cached` (both layouts through the CLOCK cache at" |
| A5 | `docs/status/format-14-kv.md`: "`efef569`: the results on the Mac and the Linux gate VM (three interleaved rounds, A/A within" |

## Result

Landed at strata:4d36aba on 2026-10-20. A 4 KiB LZ4 block miss measures 1.5 µs against 36 µs for today's 64 KiB zstd frame. No review named in the ledger.

- Merge: Merge remote-tracking branch 'origin/design/format-14-kv'.
- strata:6abf075 adds four micro-prototypes, targets, driver, and raw results.
- strata:bb78b18 adds the design and split-layout zstd rows.
- strata:785bf19 adds cached prototypes, layouts, and cache.
- strata:efef569 records Mac and Linux gate VM results and decision tables.
- Split gets are within 12%, bytes within 3%; seeks are 3× slower before termdict seek.
- Cache CLOCK has 1.35× LRU's hits on one thread and 4.5× on sixteen at the same hit rate.
- Owner's Q1/Q2 decisions and termdict prerequisites remain open for implementation.
- Docs: `docs/design/format-14-kv.md`, docs/design/format-14-kv/.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
