# phase7c-x2b: Task-based handlers and reader views

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:d25fb70a4 (2026-10-30); its ledger `docs/status/phase7c-x2b.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-x2b`.

## Outcome

Handler, schema, resolver, and view operations are tasks; shared reader views replace core.mtx and Pinned. Reader limits, reclamation, and fanout have tests and HTTP evidence.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Remove sync_wait from server request paths; remaining bridges are identified: `docs/status/phase7c-x2b.md`
- A2 [artifact] K2 Replace core.mtx and Pinned with hazard-guarded reader views: `docs/status/phase7c-x2b.md`
- A3 [artifact] K3 Reclaim retired views through housekeeping after guards release: `docs/status/phase7c-x2b.md`
- A4 [artifact] K4 Map too_many_readers to a 503 and test per-index reader limits: `docs/status/phase7c-x2b.md`
- A5 [artifact] K5 Prove Release, Debug, ASan/UBSan, TSan, E2E, recovery, and crash behavior: `docs/status/phase7c-x2b.md`
- A6 [artifact] K6 Update reader-view mechanism notes, registry/search/fields notes, and changelog: `docs/status/phase7c-x2b.md`
- A7 [artifact] R1 Use a reader registry per index: `docs/status/phase7c-x2b.md`
- A8 [artifact] R2 Keep the registry alive until every guard is released: `docs/status/phase7c-x2b.md`
- A9 [artifact] R3 Add max_indexes_per_search and return 400 above the limit: `docs/status/phase7c-x2b.md`
- A10 [artifact] R4 Document the N2 rule and why X2b satisfies it: `docs/status/phase7c-x2b.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Writer-side adapter relies on the host's turn; reads see one published view, so find_key no longer sees the write buffer.
- Closing index waits for view holders (30 s, then busy) instead of destroying under them.
- N2: the write stands and autocommit commits it; a 503 could make an auto-id client add it twice.
- Risk high: concurrent reader lifetimes and request handling (the lead's mapping).

## May land early

None.
