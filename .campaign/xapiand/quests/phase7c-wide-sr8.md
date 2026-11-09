# phase7c-wide-sr8: Restore nested indexes and glob patterns

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:1dc98c213 (2026-11-03); its ledger `docs/status/phase7c-wide-sr8.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-wide-sr8`.

## Outcome

Nested indexes are allowed, and bounded pattern walks descend into index directories without hiding child indexes. Patterns are path-segment globs: `*` stays within a segment and `**` spans segments.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Remove nesting refusal and parent reads; restore docs examples and remove creation lock/rollback: `docs/status/phase7c-wide-sr8.md`
- A2 [artifact] Walk each directory once, identify index markers, list child directories, and descend where patterns remain live: `docs/status/phase7c-wide-sr8.md`
- A3 [artifact] Glob semantics: `*`, `?`, `[...]` within a segment; `**` spans zero or more segments; `_all` is `**`: `docs/status/phase7c-wide-sr8.md`
- A4 [artifact] Count every entry read, including at depth and in index directories, against `--max-search-indexes`: `docs/status/phase7c-wide-sr8.md`
- A5 [artifact] Preserve SR8 `WalkGate`, `--search-walk-queue`, `--search-walk-wait`: `docs/status/phase7c-wide-sr8.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner ruling (2026-11-03): nested indexes are fundamental; reverse the nesting refusal.
- Use a segment automaton; traversal work remains budgeted.
- Risk high: Changes index traversal, path semantics, and search limits (the lead's mapping).

## May land early

None.
