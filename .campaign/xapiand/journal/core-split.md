# core-split: journal

State: landed
Quest: .campaign/xapiand/quests/core-split.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:3f88a15 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/core-split.md`: "headers moved into include/strata/core and include/strata/search, callers rewritten by name." |
| A2 | `docs/status/core-split.md`: "the six mixed files split at the plugin seam; basic_index names no search type" |
| A3 | `docs/status/core-split.md`: "core_boundary (every core header alone against core's include root and libraries), core_boundary_negative" |
| A4 | `docs/status/core-split.md`: "design note, AGENTS.md map, what the KV harness needs." |
| A5 | `docs/status/core-split.md`: "The get-by-key commit: basic_index::get for layouts with stored; checked in core_kv_test and index_test." |
| A6 | `docs/status/core-split.md`: "bench/kv/kv_strata.cc ported to strata::core (get(), segments() scans; kv_strata links core alone" |
| A7 | `docs/status/core-split.md`: "CN5/CN6 2130538. Release ctest 45/45 and ASan+UBSan 45/45." |
| A8 | open: CR4 open until the final tip is green on all 8 jobs in `docs/status/core-split.md` |

## Result

Landed at strata:3f88a15 on 2026-10-20. Release and ASan+UBSan ctest 45/45 after review fixes. Review: `docs/reviews/core-split-code.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge remote-tracking branch 'origin/phase4/core-split'.
- strata:298fc91 moves headers and rewrites callers.
- strata:364f013 splits six mixed files at the plugin seam.
- strata:c2aedb5 adds boundary and core KV tests.
- strata:89d2ebe adds the design note, AGENTS.md map, and harness requirements.
- Revert and mutant checks catch boundary violations, missing installs, ignored deleted bits, and unreadable structure bits.
- CR4 and CI at the final tip remain open.
- Docs: `docs/design/core-split.md`.
- Left open: A8.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
