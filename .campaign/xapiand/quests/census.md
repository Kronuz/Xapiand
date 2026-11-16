# census: Instrument engine seam usage

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:829a67308 (2026-10-27); its ledger `docs/status/census.md` is the record. Its machine state is in `state.json`: `campaign status census`.

## Outcome

`-DENGINE_CENSUS=ON` counts engine operations per HTTP request and emits JSON when configured. The default build contains no census symbols or environment-variable strings.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Add optional census counters and one JSON line per HTTP request: `docs/status/census.md`
- A2 [artifact] Count both candidate prefixes `Q` and `_id.` in `census::document`: `docs/status/census.md`
- A3 [artifact] Run census E2E and record results in `docs/dev/engine-census.md`: `docs/status/census.md`
- A4 [artifact] Test census behavior and confirm default binary omits census symbols and strings: `docs/status/census.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Count the T5 check without asserting it.
- Risk low: Optional instrumentation and measurement tooling (the lead's mapping).

## May land early

None.
