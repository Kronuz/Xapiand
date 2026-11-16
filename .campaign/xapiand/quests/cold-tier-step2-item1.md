# cold-tier-step2-item1: Lease ownership and close readiness

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:8d7467b (2026-11-02); its ledger `docs/status/cold-tier-step2.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-step2-item1`.

## Outcome

`write_lease::held_here()` and a shared holder word let records and moved indexes identify lease ownership safely. `close_ready()` combines queued reclaim, uncommitted work and warm-state checks before an index can close.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] S1 B's record mapped inside A's chore counts on B; own lease/chore maps and promotion maps do not count: `docs/status/cold-tier-step2.md`
- A2 [artifact] S1 lease-check-off, per-thread and per-move behavior is tested: `docs/status/cold-tier-step2.md`
- A3 [artifact] S2 Queued and posted reclaim prevent close until deletion completes; close accounts for remaining budget: `docs/status/cold-tier-step2.md`
- A4 [artifact] S2 Nothing-uncommitted clause refuses buffered, flushed-unsynced and delete-alone work: `docs/status/cold-tier-step2.md`
- A5 [artifact] S2 Warm clauses include owed turn, pending merge, unsynced merge output and closed-index behavior: `docs/status/cold-tier-step2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- `load_context` retains a lease holder word, not a pointer to the index's `write_lease`.
- `close_ready()` is a chore that throws on a closed index; warm readiness is factored into `warm_quiescent()`.
- Debt is zero when reclaim is empty; its separate readiness clause is belt-and-braces.
- Risk high: Changes shared lease-word ownership and concurrent index lifecycle checks (the lead's mapping).

## May land early

None.
