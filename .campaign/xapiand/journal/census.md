# census: journal

State: landed
Quest: .campaign/xapiand/quests/census.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:829a67308 on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/census.md`: "`-DENGINE_CENSUS=ON` (off by default): counters at the seam, one JSON line per HTTP request" |
| A2 | `docs/status/census.md`: "T5 check in `census::document`, counted not asserted, for both candidate prefixes `Q` and `_id.`" |
| A3 | `docs/status/census.md`: "E2E run with the census build: 339 requests, `e2e_assert.py` GREEN" |
| A4 | `docs/status/census.md`: "Default build unchanged: `nm` finds 0 census symbols and `strings` 0 `XAPIAND_ENGINE_CENSUS`" |

## Result

Landed at Xapiand:829a67308 on 2026-10-27. E2E: 339 requests; `e2e_assert.py` GREEN. No review named in the ledger.

- Merge: Merge branch 'phase0.5/census' (gate 0.5 T1: engine census).
- Xapiand:cdfdba558 and follow-up commit -- census hooks, candidate prefixes, report, and docs.
- `test_engine_census` 67 checks; engine search 4417, shard 52, read 657, query 22395 checks pass.
- Mutants for callback, T5 guard, and search hook each caught.
- Default binary: 0 census symbols and 0 census strings; `__TEXT` 6635520, `__DATA` 2220032.
- Census build: 8657024 bytes (+34992); seam check 7 files, 158 lines.
- Open: no second capture, multi-node census, or CI.
- Docs: `docs/dev/engine-census.md`, `docs/mechanisms/engine-census.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
