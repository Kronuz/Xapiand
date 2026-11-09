# x1-delete-xapian: Remove Xapian and cluster code

Risk: critical. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:0ae3505c9 (2026-10-29); its ledger `docs/status/x1-delete-xapian.md` is the record. Its machine state is in `state.json`: `campaign status x1-delete-xapian`.

## Outcome

The Xapian engine, adapter, remote protocols, replication, and cluster-only features are removed. A one-node strata-backed registry and engine remain; Release and Debug builds, tests, and docs E2E pass.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Remove src/xapian and Xapian includes, links, options, and dependencies: `docs/status/x1-delete-xapian.md`
- A2 [artifact] K2 Remove the adapter and engine switch; retain the listed engine duties through strata: `docs/status/x1-delete-xapian.md`
- A3 [artifact] K3 Remove remote, replication, election, and cluster-only code and documentation: `docs/status/x1-delete-xapian.md`
- A4 [artifact] K4 Keep sortable_serialise while it has users; leave it unopened: `docs/status/x1-delete-xapian.md`
- A5 [artifact] K5 Release and Debug builds, unit tests, docs E2E, and an ASan/UBSan build: `docs/status/x1-delete-xapian.md`
- A6 [artifact] K6 Update changelog, extraction ledger, README, ARCHITECTURE, and AGENTS: `docs/status/x1-delete-xapian.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead may overrule: --shards stays; number_of_replicas remains accepted and stored but has no one-node effect.
- GET / loses cluster_name, _id, and _version; keeps name.
- ?volatile stays because it opens the writer handle.
- engine::serialise_double changes wire form for transient aggregation merge and key-maker round trips.
- Risk critical: removes user-visible cluster and Xapian behavior (the lead's mapping).

## May land early

None.
