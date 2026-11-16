# gate7a-fixes: Gate fixes and feature removals

Risk: critical. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:8fb9032a2 (2026-10-28); its ledger `docs/status/gate7a-fixes.md` is the record. Its machine state is in `state.json`: `campaign status gate7a-fixes`.

## Outcome

Strata writes stamp the landed version, engine checks dispatch by index kind, and stats include document-length bounds. ELITE_SET, text `_terms`, and INFO term lists are removed; the PHRASE-window request remains refused.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] R1 Stamp version slot at all three strata write sites: `docs/status/gate7a-fixes.md`
- A2 [artifact] D5 Ignore non-joining operator on append to empty accumulation: `docs/status/gate7a-fixes.md`
- A3 [artifact] D1 Dispatch `DatabaseHandler::check` through `engine::check(path)`: `docs/status/gate7a-fixes.md`
- A4 [artifact] D3 Calculate document-length bounds from segments with documents: `docs/status/gate7a-fixes.md`
- A5 [artifact] 5 Remove ELITE_SET: `docs/status/gate7a-fixes.md`
- A6 [artifact] 6 `_terms` reads stored values only; text fields or fields without values answer 400: `docs/status/gate7a-fixes.md`
- A7 [artifact] 7 Remove INFO term lists on both engines: `docs/status/gate7a-fixes.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead rule: remove features strata will not serve naturally, rather than refuse them.
- No decision recorded for PHRASE windows beyond its existing refusal.
- Risk critical: changes version handling and removes user-visible features (the lead's mapping).

## May land early

None.
