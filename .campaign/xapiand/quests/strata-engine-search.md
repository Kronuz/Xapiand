# strata-engine-search: Strata search engine

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:4caf46c9b (2026-10-28); its ledger `docs/status/strata-engine-search.md` is the record. Its machine state is in `state.json`: `campaign status strata-engine-search`.

## Outcome

Strata searches support the recorded term, boolean, phrase, near, and append query forms, with estimates, ordering, and spies. Unsupported query shapes return a client error; term lists and values remain limited as recorded.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] 1. `engine::search` dispatches on shard engine; mixed-engine search is refused (`invalid`): `docs/status/strata-engine-search.md`
- A2 [artifact] 2. Supports terms, `match_all`, AND, OR, AND_NOT, AND_MAYBE, FILTER, SCALE_WEIGHT, NEAR, PHRASE over terms, and append chains: `docs/status/strata-engine-search.md`
- A3 [artifact] 3. One `prospect::multi_snapshot` spans every segment of every shard; uses global statistics, `bm25_xapian`, and the recorded collection behavior: `docs/status/strata-engine-search.md`
- A4 [artifact] 4. Estimate is exact live-match count; write buffer flushes before search: `docs/status/strata-engine-search.md`
- A5 [artifact] 5. Ties break by docid in requested order: `docs/status/strata-engine-search.md`
- A6 [artifact] 6. Recorded unsupported shapes return `errc::syntax`; PHRASE terms retain query-position offsets: `docs/status/strata-engine-search.md`
- A7 [artifact] 7. A spy sees every live match once: `docs/status/strata-engine-search.md`
- A8 [artifact] 8. Shared Xapian tests, docs suite, ASan and UBSan, mutants, smoke; `docs/mechanisms/strata-search.md`: `docs/status/strata-engine-search.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead's note, 2026-10-27: key prefix comes from `keys::key_prefix()`; `term_key_prefix()` is the seam to change after fields-and-keys.
- Refusal kind is `errc::syntax`, a client error, not retried.
- Search pins every shard's lock in address order for the full search; T10 replaces this with a snapshot that outlives the lock.
- Risk normal: adds search behavior and refusals (the lead's mapping).

## May land early

None.
