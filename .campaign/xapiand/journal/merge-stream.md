# merge-stream: journal

State: landed
Quest: .campaign/xapiand/quests/merge-stream.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:6589f32 on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/merge-stream.md`: "Stream the stored section: `stored_builder::stream_to` writes each block's frame through the section stream" |
| A2 | `docs/status/merge-stream.md`: "Key-ordered merge plan: numbered straight from the order (no docid-sorted copy, no refs copy)" |
| A3 | `docs/status/merge-stream.md`: "test/merge_stream_test.cc pins merged-file digests of five shapes from main's whole-section merge" |
| A4 | `docs/status/merge-stream.md`: "Release ctest 38/38; ASan+UBSan 38/38; TSan 38/38, no reports." |

## Result

Landed at strata:6589f32 on 2026-10-21. Release ctest 38/38; ASan+UBSan 38/38; TSan 38/38, no reports. No review named in the ledger.

- Merge: Merge remote-tracking branch 'origin/perf/merge-stream'.
- `merge_stream_test` pins merged-file digests of five shapes and bounds merge budget peak.
- Peak RSS 334.6 -> 159.4 MiB (fillrandom), 364.8 -> 192.0 (readrandom), 674.6 -> 230.4 (ycsb_a 1 KiB).
- Budget peak 96 -> 38, 106 -> 50, 210 -> 11 MiB; throughput flat or up.
- fits1 RSS 61 -> 49, 69 -> 55, 312 -> 190; digests identical.
- Key-builder transient memory, Linux/FreeBSD runs, and mapped-pages residency measure remain open.
- Docs: ledger names `docs/perf/bigmem.md` and `docs/perf/merge-stream.md`, but the repository glob found no matching docs files.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
