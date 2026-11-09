# reclaim-bg: Background file reclamation

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:45e6b32 (2026-10-26); its ledger `docs/status/reclaim-bg.md` is the record. Its machine state is in `state.json`: `campaign status reclaim-bg`.

## Outcome

With a host executor, reclaim posts truncate, sync, and unlink work off the owner thread; in-flight bytes remain covered by the storage permit. Without an executor, deletion remains inline.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Tests cover posted deletion, failure and requeue, drain and close waits, separation from merges, and crash recovery: `docs/status/reclaim-bg.md`
- A2 [artifact] Revert checks and mutants catch incorrect accounting, swallowed failure, and missing drain wait: `docs/status/reclaim-bg.md`
- A3 [artifact] Full ctest passes under ASan+UBSan and TSan: `docs/status/reclaim-bg.md`
- A4 [artifact] A/B compares run-phase ratio, settled disk, and get digest across at least 5 interleaved rounds: `docs/status/reclaim-bg.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- CR2: the host must keep the pool running until indexes close; waits have no timeout or warning.
- CR4: no in-flight byte cap without a storage budget; with a budget, admission drains and waits.
- CR7: `wait_deletes()` collects but does not sync the directory; it is an owner-thread call.
- Risk high: Changes asynchronous file deletion, storage accounting, and close behavior (the lead's mapping).

## May land early

None.
