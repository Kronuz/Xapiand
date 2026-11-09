# strata-engine-write: journal

State: landed
Quest: .campaign/xapiand/quests/strata-engine-write.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:5d7f3263d on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/strata-engine-write.md`: "transactions (cancel reopens from the last commit). Done." |
| A2 | `docs/status/strata-engine-write.md`: "add, replace, erase, by docid and by key. Done." |
| A3 | `docs/status/strata-engine-write.md`: "Version check-and-set from strata's version map, no `V` terms, no version slot. Done." |
| A4 | `docs/status/strata-engine-write.md`: "Stats: uuid, revision, lastdocid, doc count, total length. Done;" |
| A5 | `docs/status/strata-engine-write.md`: "`DocView`: did, version, data. Values are TODO-gated on P1" |
| A6 | `docs/status/strata-engine-write.md`: "WAL replay through `Document::unserialise(kind, ...)`: done, through a node (below)." |
| A7 | `docs/status/strata-engine-write.md`: "Hits by docid: `get(did)` and `scan(from, sink)`. Done." |
| A8 | open: partial: D8's saved `IndexSettings` field not done in `docs/status/strata-engine-write.md` |
| A9 | `docs/status/strata-engine-write.md`: "`harness/seam_check.sh` unchanged (only `src/engine/strata/` names strata or prospect)" |

## Result

Landed at Xapiand:5d7f3263d on 2026-10-28. `ctest` 21 of 21; `test_engine_strata`: 8081 checks. Review: `docs/reviews/strata-engine-write-code.md` named; ledger says approve with conditions, awaiting re-check.

- Merge: Merge branch 'phase7a/strata-engine-write' (S1: strata behind the engine seam).
- Xapiand:75178826d the engine, `Document(Kind)`/`set_key`, `kind_of`, and `open_local(kind, ...)`.
- Xapiand:b078d5a18 tests on both engines, codec fuzz target, pins, and harness.
- Xapiand:9cbd16f51 idempotent replay, crash points, and S1 proof tests; Xapiand:a73ea8d18 crash scenarios and model client.
- Node proof: 30 PUTs recovered across crashes; all 30 served.
- Fuzz: 808144 runs in 61 s, no finding; ASan+UBSan `test_engine_strata` 8081 of 8081.
- Bulk screen: Xapian 4608, 4595, 4595 docs/s; strata 5200, 5185, 5363 docs/s; whole pipeline 1.14 times faster, 0.77 of the CPU; Open: T11 measurement with equivalent work, D8 `IndexSettings`, values/term lists, and review CR8 to CR10.
- Docs: `docs/mechanisms/strata-engine.md` named.
- Left open: A8.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
