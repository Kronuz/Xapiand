# name-index: Index patterns resolve from an ordered name index

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status name-index`.

## Outcome

Index patterns resolve names from an ordered name index (a prefix is a range scan, O(log n +
matches)) kept by the catalog's open and create paths or a persisted names table, covering nested
paths, so a node with millions of indexes can take a wildcard. The bounded directory walk stays
as the fallback until the index is complete (carried "Index patterns at scale", wide-search SR1,
2026-11-03).

## Acceptance criteria

- A1 [artifact] A design for the name index: where it lives (catalog paths or a persisted table), how it covers nested paths (D22), its cost per index and per lookup, and its crash behavior (carried "Index patterns at scale"): `docs/design/name-index.md`
- A2 A prefix pattern over N indexes costs O(log N + matches), measured at 10^4, 10^5 and 10^6 indexes against the walk: jobs
- A3 Every pattern answer equals the walk's on the same tree, nested paths included: a property test in the `build-test` check
- A4 A node with more indexes than `--max-search-indexes` answers a wildcard: a test
- A5 Crash: a kill during create leaves the name index consistent with the data directory after restart: a test

## Contracts

Consumes `x-pool`'s catalog paths (open and create). Provides pattern resolution to `search_batch`;
the walk's answers stay the specification.

## Decisions

- Designed "with X-POOL step 2 or later" (road-to-release): `--after x-pool`.
- Open (owner): whether it gates the release. road-to-release records the limit ("doesn't fit
  millions of indexes per host") but does not place the fix before Phase 8.
- A design review comes first (persistent state); the file name in A1 is the lead's choice.

## May land early

The design document.
