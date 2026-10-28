# strata-engine-write (S1, phase 7a)

Branch `phase7a/strata-engine-write` from Xapiand master `eb83813a8`. Local only, not pushed, no CI run.
Design: `strata/docs/design/xapiand-on-strata.md` (strata:9dc5fd3), row S1, D2 to D13, gate rows T6 to T11.

## State

In progress: writing the engine (first build pending).

## Done criteria (design row S1)

`engine::strata::Shard`: lifecycle, add, replace, erase, by key, version check, metadata, stats, `DocView` data and
values; WAL replay; hits by docid. The engine is chosen per index. `harness/seam_check.sh` stays green.

1. Lifecycle (open, create, close, reopen, commit, transactions).
2. add, replace, erase, by docid and by key.
3. Version check-and-set from strata's version map (no `V` terms).
4. Metadata get, set, keys. Stats.
5. `DocView` data and values (values TODO-gated on prospect P1, `phase4/values-bytes`, not landed).
6. WAL replay through `Document::unserialise()`.
7. Hits by docid.
8. Engine per index (on-disk detection plus `XAPIAND_ENGINE`; an `IndexSettings` field is the remaining half of D8).
9. Tests on both engines; ASan; WAL replay checks; first bulk screen against Xapian.
10. `docs/mechanisms/strata-engine.md`.

## Done

(nothing committed yet)

## Next

Build, unit tests, ASan, harness runs, bulk screen, mechanism note.

## Blocked on

Nothing.
