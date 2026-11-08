# phase7c-x6: Remove threads and enforce blocking gate

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:2ff594ebe (2026-11-02); its ledger `docs/status/phase7c-x6.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-x6`.

## Outcome

Replaced thread options and obsolete offload machinery are removed, while debug destructor assertions catch outstanding work. Shutdown follows one deadline, and CI runs strict blocking and removed-option checks.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Remove thread options, obsolete stats/gauges, Hop, and replaced offload machinery: `docs/status/phase7c-x6.md`
- A2 [artifact] K2 Assert on destruction with references, turns, views, chores, or detached tasks outstanding: `docs/status/phase7c-x6.md`
- A3 [artifact] K3 Stop HTTP service, autocommits, indexes, WALs, and fsync threads under one bound: `docs/status/phase7c-x6.md`
- A4 [artifact] K4 Run async_check.sh --strict and removed_options_check.sh in CI: `docs/status/phase7c-x6.md`
- A5 [artifact] K5 Update architecture, internals, control-plane note, and options documentation: `docs/status/phase7c-x6.md`
- A6 [artifact] K6 Prove Release, Debug, ASan/UBSan, TSan, E2E, shutdown, recovery, and CLI checks: `docs/status/phase7c-x6.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Keep --strict, not --strict --owed, until X1 and X4 land and remaining rows are reviewed.
- D16's acceptor-close order differs from code; retain current graceful wait pending lead decision.
- --processors sets reactor count without a cap; fsync pool is ceil(0.5 x processors), 1 to 10.
- Risk high: concurrency, durability, and shutdown behavior (the lead's mapping).

## May land early

None.
