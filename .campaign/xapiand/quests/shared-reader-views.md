# shared-reader-views: Publish and reclaim guarded reader views

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:23e78bb (2026-10-26); its ledger `docs/status/shared-reader-views.md` is the record. Its machine state is in `state.json`: `campaign status shared-reader-views`.

## Outcome

The index publishes guarded reader views, tracks and retires them, and retains referenced segments until safe reclamation. Owner lease checks protect write-side duties, and close can defer completion while readers remain.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Items 3 and 5: implement view, reader handles, publication, retirement, retention, reclamation, close, and move-assignment behaviors: `docs/status/shared-reader-views.md`
- A2 [artifact] K2 Close SR-D19 and SR-D20 with revert checks for lease primary, mark second, holder token, and publish-through coalescing: `docs/status/shared-reader-views.md`
- A3 [artifact] K3 Kill all 15 listed mutants: `docs/status/shared-reader-views.md`
- A4 [artifact] K4 Pass ASan+UBSan and TSan ctest; verify NDEBUG behavior: `docs/status/shared-reader-views.md`
- A5 [artifact] K5 Green CI: `docs/status/shared-reader-views.md`
- A6 [artifact] K6 Document public API: `docs/status/shared-reader-views.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- SR-D20: debug-checked holder token for owner duties; release builds keep no token.
- SR-D19: lease is primary owner check in debug; worker mark stays as second check in every build.
- Risk high: Adds lock-free shared-reader publication, leases, and reclamation (the lead's mapping).

## May land early

None.
