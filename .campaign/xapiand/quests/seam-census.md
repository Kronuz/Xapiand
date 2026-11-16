# seam-census: Correct numeric ID accuracy prefix

Risk: critical. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:d0b43fad2 (2026-10-27); its ledger `docs/status/seam-census.md` is the record. Its machine state is in `state.json`: `campaign status seam-census`.

## Outcome

Census reads native document terms and values only in census builds, with no census symbols in the default build. Numeric `_id` accuracy terms use prefix `A`, preserving range-query answers while correcting the T5 count.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Merge seam-write and add census access to native documents only under `ENGINE_CENSUS`: `docs/status/seam-census.md`
- A2 [artifact] A-T5: use `A` for numeric `_id` accuracy terms and preserve `_id` range queries: `docs/status/seam-census.md`
- A3 [artifact] (seam-write) Build native-backed `engine::Document`; shard, log, and word-break terms use it: `docs/status/seam-write.md`
- A4 [artifact] (seam-write) Preserve SEARCH dump, `:info`, E2E, unit tests, WAL recovery, and seam check: `docs/status/seam-write.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Numeric `_id` accuracy terms use `A`; existing numeric `_id` indexes need reindexing for range queries.
- Risk critical: Existing numeric-ID indexes require reindexing for range queries (the lead's mapping).

## May land early

None.
