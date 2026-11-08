# pool-concurrency: Pool cleanup and wakeup fixes

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:2466dac5c (2026-10-26); its ledger `docs/status/pool-concurrency.md` is the record. Its machine state is in `state.json`: `campaign status pool-concurrency`.

## Outcome

Pool cleanup serializes runtime evictions, and checkin, unlock, and clear coordinate shard state with waiters. `Shard::reopen` stops after a successful open; shutdown and committer-lifetime concerns remain follow-ups.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Reproduce the UAF, lost wake-ups, and repeated reopen on master with tests and root causes: `docs/status/pool-concurrency.md`
- A2 [artifact] K2 Give each fix a failing-before test; include revert checks and mutants: `docs/status/pool-concurrency.md`
- A3 [artifact] K3 UAF hunt clean for 30 rounds; lost-wakeup hammer 0 of 30 rounds; reopen opens once: `docs/status/pool-concurrency.md`
- A4 [artifact] K4 TSan `test_pool` 3 runs, 0 warnings, 12 checks; docs E2E GREEN on master and fix, 0 ASan reports: `docs/status/pool-concurrency.md`
- A5 [artifact] K5 Add two bullets under `[Unreleased]` / Fixed: `docs/status/pool-concurrency.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Reviewer confirmed CR1 and CR2 CLOSED on 2026-10-26.
- Lead merged the changes into local master.
- CR3-CR7 remain follow-ups or require a probe.
- Risk high: Shared pool concurrency and shard lifecycle changed (the lead's mapping).

## May land early

None.
