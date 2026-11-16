# engine-search: Engine reads, search, and error kinds

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:cca0011bb (2026-10-27); its ledger `docs/status/engine-search.md` is the record. Its machine state is in `state.json`: `campaign status engine-search`.

## Outcome

Engine reads and searches return neutral results, while engine errors carry kinds and preserve HTTP mapping. The seam closes outside the remote-protocol floor, and malformed text queries now answer 400 rather than 500.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] X4a: Add task-based engine shard reads, `DocView` methods, Xapian check, and native access: `docs/status/engine-search.md`
- A2 [artifact] X4a: Route shard reads and named dump/info/check/document-info callers through the seam: `docs/status/engine-search.md`
- A3 [artifact] X4a: Add CI seam check with shrink-only file ceilings: `docs/status/engine-search.md`
- A4 [artifact] X4a: Preserve behavior across unit tests, E2E, qdiff, qnf, qread, and cluster checks: `docs/status/engine-search.md`
- A5 [artifact] X4a: Revert-check each adapter and test view ownership: `docs/status/engine-search.md`
- A6 [artifact] X4b: Add engine search request/results and move Enquire, RSet, and ESet work to Xapian implementation: `docs/status/engine-search.md`
- A7 [artifact] X4b: Migrate match and MSet consumers; remove `get_rset`, `get_edecider`, and MIT `Shard::db()` use: `docs/status/engine-search.md`
- A8 [artifact] X4b: Preserve corpus, E2E, and remote-protocol search behavior; test ownership and adapters: `docs/status/engine-search.md`
- A9 [artifact] X4c: Translate errors to engine kinds; centralize read retry and missing-data policy: `docs/status/engine-search.md`
- A10 [artifact] X4c: Preserve HTTP mapping, migrate Xapian spellings/includes and text-analysis pieces, retain documented GPL floor: `docs/status/engine-search.md`
- A11 [artifact] X4c: Test error mappings, retries, DocView translation, seam floor, and review conditions: `docs/status/engine-search.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Add `errc::syntax`; preserve Xapian description and OS detail on engine errors.
- Any residual Xapian error maps to `internal`; DocView methods throw `engine::Error` only.
- Unify `DatabaseModified` read behavior on retry and close on the last attempt.
- Preserve Xapian-derived exception bases where remote/replication code still catches or throws them.
- Risk high: Migrates shared search APIs, error mapping, and shard reads (the lead's mapping).

## May land early

None.
