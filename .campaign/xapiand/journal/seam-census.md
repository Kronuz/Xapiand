# seam-census: journal

State: landed
Quest: .campaign/xapiand/quests/seam-census.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:d0b43fad2 on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/seam-census.md`: "The census reads a document through `Document::Native::census_terms` and `census_values`" |
| A2 | `docs/status/seam-census.md`: "A-T5 (Xapiand:be13bc961). A numeric `_id`'s accuracy terms lead with `A`" |
| A3 | `docs/status/seam-write.md`: "Native-backed `engine::Document`; shard, log and word-break terms use it (commit below)." |
| A4 | `docs/status/seam-write.md`: "SEARCH dump and `:info` identical to `cca0011bb`, docs E2E 576/576 (newman 6.2.3) on Release and ASan+UBSan" |

## Result

Landed at Xapiand:d0b43fad2 on 2026-10-27. Default binary: 0 census symbols and 0 census strings; `__TEXT` 6627328, `__DATA` 2220032. Review (not named in the ledger): `docs/reviews/seam-write-code.md` -- APPROVE WITH CONDITIONS (seam-write, merged into this quest).

- Merge: Merge branch 'phase0.5/seam-census' (write path, census port, A-T5).
- Xapiand:03330fe46 -- merge; Xapiand:be13bc961 -- A-T5 correction.
- Census build: 8651200 bytes against 8613120 for seam-write.
- Census suite: 2625 documents; 0 with more than one `Q` term; 473 named-key and 2152 keyless each hold exactly one.
- ASan+UBSan E2E: 576 of 576; ctest 19 of 19 under ASan+UBSan; Release 18 of 19.
- SEARCH and `:info` hashes identical; five `_id` range queries hash identically; 20000-document bulk smoke 4344 and 4491 docs/s.
- Open: review and strata implementation; numeric `_id` indexes need reindexing.
- Docs: `docs/mechanisms/engine-write-path.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
