# prospect-key-field: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-key-field.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:44415b6 on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/key-field.md`: "`inverted_plugin::check` and `values_plugin::check` lose the prefix argument" |
| A2 | `docs/status/key-field.md`: "`test/format13_test.cc`: every first byte 0..255 is a key that is also a text term" |
| A3 | `docs/status/key-field.md`: "Mutants V and W (`tools/mutants_fields.py`) pick the key leaf by the byte `Q` or `0xff` instead of the role" |
| A4 | `docs/status/key-field.md`: "The F20 manifest tests changed with the rule" |
| A5 | `docs/status/key-field.md`: "ctest 49 of 49 each (TSan skips `recycled`, as before). CI: `prospect:d00f682` 8 of 8 jobs green" |
| A6 | open: named as next; not recorded in `docs/status/key-field.md` |

## Result

Landed at prospect:44415b6 on 2026-10-28.

- Companion: strata's `key-field` history quest.
- Review: none in prospect.
- Docs: `docs/mechanisms/fields.md`.
- Left open: A6.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
