# strata-pins-2026-11: journal

State: landed
Quest: .campaign/xapiand/quests/strata-pins-2026-11.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:f2d7626 on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/pins-2026-11.md`: "Pins moved (old to new), all in CMakeLists.txt:" |
| A2 | `docs/status/pins-2026-11.md`: "fusefilter a42bd15 to 0665ee0 is this branch's one commit (`format::pre_freeze` 0x8000000C to 0x8000000D" |
| A3 | `docs/status/pins-2026-11.md`: "The resolved tree (`_deps/*-src`) was compared with each library's head; the only differences are" |
| A4 | `docs/status/pins-2026-11.md`: "Release build, `ctest`: 60/60 pass at the new pins (fusefilter held)." |

## Result

Landed at strata:f2d7626 on 2026-11-02. Release ctest: 60/60 pass with fusefilter held. No review named in the ledger.

- Merge: Merge branch 'deps/pins-2026-11-fusefilter' (fusefilter head; pre-freeze segment version bump).
- Eleven library pins are listed as moved; hashes and outcomes are recorded in the ledger.
- Hot-path comparison: 12 phase rows; geomean 1.016 (B faster), A/A 0.996, rows 0.996 to 1.043.
- Separate fusefilter branch with a42bd15 changes pre-freeze 0x8000000C to 0x8000000D; ctest 60/60.
- Format ruling and prospect repin remain open.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
