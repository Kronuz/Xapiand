# strata-engine-text: Host-side text query parsing

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:7c5dca498 (2026-10-28); its ledger `docs/status/strata-engine-text.md` is the record. Its machine state is in `state.json`: `campaign status strata-engine-text`.

## Outcome

The host-side parser lowers text queries into engine queries; the `Text` seam node and Xapian parser path are removed. Query, DSL, JSON, MessagePack, and YAML limits reject excessive inputs with 400; YAML anchors and aliases are refused.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `engine::parse_text` reads a field's query string into an `engine::Query`; `Text` seam node and Xapian `QueryParser` path are removed: `docs/status/strata-engine-text.md`
- A2 [artifact] `WORD_BREAKS` (`cjk_words`) is not read; CJK query compatibility remains open: `docs/status/strata-engine-text.md`
- A3 [artifact] Fuzz and tests cover lowering, query limits, request-body depth, and YAML anchor/alias refusal: `docs/status/strata-engine-text.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Syntax, unsupported, and too_long become `QueryParserError` (`errc::syntax`, 400); `out_of_memory` is `std::bad_alloc`.
- Unknown stemmer language or unsupported default operator becomes `InvalidTextConfigError` (`errc::invalid`).
- `WORD_BREAKS` is not read; lead may choose an ICU break-iterator segmenter.
- Lead ruling: YAML request bodies may not use anchors or aliases.
- Risk high: adds query-depth and request-body security limits (the lead's mapping).

## May land early

None.
