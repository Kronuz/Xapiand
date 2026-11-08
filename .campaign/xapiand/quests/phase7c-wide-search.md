# phase7c-wide-search: Bound searches across multiple indexes

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:4f1cc9a94 (2026-11-03); its ledger `docs/status/phase7c-wide-search.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-wide-search`.

## Outcome

Searches expand names and path-glob patterns in a bounded query chore, with cancellation, deduplication, and a one-walk gate. Search results preserve per-index relevance weights and exact ordering across batch sizes, while 64-bit hit ids support large shard sets.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Remove `--max-indexes-per-search`; add `--concurrent-search-indexes` and `--max-search-indexes`, docs and changelog: `docs/status/phase7c-wide-search.md`
- A2 [artifact] Walk endpoints in whole-index batches, at most K shards, merging top-k after each batch behind `engine::BatchRunner`: `docs/status/phase7c-wide-search.md`
- A3 [artifact] Identity test: same hits, ids, order, ranks, weights, percents, totals and spy results across specified shard counts and K values: `docs/status/phase7c-wide-search.md`
- A4 [artifact] Document weight matches its index-alone search: `docs/status/phase7c-wide-search.md`
- A5 [artifact] Hit ids are 64 bits; test old 32-bit edge and decode: `docs/status/phase7c-wide-search.md`
- A6 [artifact] Add pattern glob, expansion and limit tests, fuzz target, and E2E examples: `docs/status/phase7c-wide-search.md`
- A7 [artifact] `wide_search_check.py check` validates list, pattern and cap paths: `docs/status/phase7c-wide-search.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Use patterns, not a body list, for long index lists.
- Keep K default tied to pool share; `readers_per_index` limits readers separately.
- Allow no-match patterns to return 200 with no hits.
- Risk high: Changes shared search APIs, fanout limits, and index traversal (the lead's mapping).

## May land early

None.
