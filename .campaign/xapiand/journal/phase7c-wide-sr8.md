# phase7c-wide-sr8: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-wide-sr8.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:1dc98c213 on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-wide-sr8.md`: "`check_new_index`, `check_created`, `OpenOptions::on_create`, the creation lock and rollback in `acquire`" |
| A2 | `docs/status/phase7c-wide-sr8.md`: "`Walk::scan` reads a directory once, takes `strata` and `.__N` as the index itself" |
| A3 | `docs/status/phase7c-wide-sr8.md`: "`*`, `?`, `[...]` inside one segment (never `/`), a segment `**` is any number of segments" |
| A4 | `docs/status/phase7c-wide-sr8.md`: "every entry read, at any depth and inside index directories, counts against `--max-search-indexes`" |
| A5 | `docs/status/phase7c-wide-sr8.md`: "SR8 (`WalkGate`, `--search-walk-queue`, `--search-walk-wait`) is untouched." |

## Result

Landed at Xapiand:1dc98c213 on 2026-11-03. Release ctest 37 of 37; docs E2E 396 requests, 666 of 666; `async_check` 0 owed. Review: `docs/reviews/phase7c-wide-search-security.md` -- re-check pending.

- Merge: Merge branch 'fix/wide-search-sr8' (walk queue cap and deadline; nested indexes with path globs).
- `Xapiand:708b6bfe3` removes refusal and implements nested walk/globs; `Xapiand:c8df312fa` updates docs.
- ASan+UBSan tests pass; glob and expansion fuzz campaigns report no finding.
- Nested-tree SR1 probes: 100,441-entry tree, cap 50,000; broad patterns return 400 in 1.0 to 1.3 s.
- Capacity cost rises to about 2 entries per one-shard index; default adjustment remains for lead.
- Linux/FreeBSD/TSan, wide-search harness and security re-check remain open.
- Docs: `docs/mechanisms/wide-search.md`, `docs/dev/testing.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
