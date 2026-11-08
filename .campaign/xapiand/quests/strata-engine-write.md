# strata-engine-write: Strata write engine and replay

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:5d7f3263d (2026-10-28); its ledger `docs/status/strata-engine-write.md` is the record. Its machine state is in `state.json`: `campaign status strata-engine-write`.

## Outcome

The strata engine supports writes, version checks, metadata, document-id reads, and WAL replay. Its local proof covers crash recovery, concurrency, and sanitizers; values and term-list APIs remain unavailable.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] 1. Lifecycle: open, create, close, reopen, commit, and transactions; cancel reopens from the last commit: `docs/status/strata-engine-write.md`
- A2 [artifact] 2. Add, replace, and erase by docid and by key: `docs/status/strata-engine-write.md`
- A3 [artifact] 3. Version check-and-set from strata's version map, without `V` terms or a version slot: `docs/status/strata-engine-write.md`
- A4 [artifact] 4. Metadata get, set, and keys; stats include uuid, revision, lastdocid, doc count, and total length: `docs/status/strata-engine-write.md`
- A5 [artifact] 5. `DocView`: did, version, data; values, terms, term_details, and serialise have the recorded TODO-gated behavior: `docs/status/strata-engine-write.md`
- A6 [artifact] 6. WAL replay through `Document::unserialise(kind, ...)`, through a node: `docs/status/strata-engine-write.md`
- A7 [artifact] 7. Hits by docid through `get(did)` and `scan(from, sink)`: `docs/status/strata-engine-write.md`
- A8 [artifact] 8. Engine per index: `engine::kind_of(path)`; saved `IndexSettings` field of D8 is not done: `docs/status/strata-engine-write.md`
- A9 [artifact] 9. `harness/seam_check.sh` unchanged; other `Xapian::` counts did not move: `docs/status/strata-engine-write.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead ruling requested: key prefix `Q` cannot be used; use `\xff` plus the id term.
- No decision recorded for the requested architect ruling on `Q`.
- Risk high: durable writes, replay, and concurrent updates (the lead's mapping).

## May land early

None.
