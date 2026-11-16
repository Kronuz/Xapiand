# cold-tier-step2-item2: Counted IO resource budgets

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:033e546 (2026-11-02); its ledger `docs/status/cold-tier-step2.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-step2-item2`.

## Outcome

`resource_budget` and `counted_io` reserve maps, descriptors and vnodes before kernel calls. Failed operations return reservations, external failures lower soft caps, and listeners notify catalogs when caps are crossed.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] S3 Hard caps refuse before kernel calls; reservations are returned on failures, exceptions and lost install races: `docs/status/cold-tier-step2.md`
- A2 [artifact] S3 Counts match kernel resources at quiescent points, including across catalogs sharing a budget: `docs/status/cold-tier-step2.md`
- A3 [artifact] S3 Directory descriptors count; process limits and external pressure/recovery rules are tested: `docs/status/cold-tier-step2.md`
- A4 [artifact] S3 Crossing posts to every listener: `docs/status/cold-tier-step2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Reservations use compare-exchange loops so observed counts never exceed hard caps.
- Linux uses map caps; other platforms use vnode caps; descriptor caps apply everywhere.
- `tick()` raises caps by compare-exchange; `note_failure()` applies the external cap floor.
- Risk high: Shared concurrent resource reservations enforce system-call caps (the lead's mapping).

## May land early

None.
