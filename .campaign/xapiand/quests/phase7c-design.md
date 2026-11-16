# phase7c-design: Phase 7c design revision

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:05c8dc8ff (2026-10-29); its ledger `docs/status/phase7c.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-design`.

## Outcome

Revision 2 specifies the Phase 7c design and records answers to Q1-Q6. It closes R1-R7 and assigns criteria to S1 and X1.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Revision 1 of the design and this ledger: Xapiand:0a8d235b8, Xapiand:fdcc91862: `docs/status/phase7c.md`
- A2 [artifact] Architect's preliminary review, APPROVE WITH CONDITIONS for S1 only: Xapiand:e30a844ff: `docs/status/phase7c.md`
- A3 [artifact] Revision 2: Xapiand:2d8c2daab: `docs/status/phase7c.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner (2026-10-29, `docs/design/phase7c.md` section 6): Q1 delete the Xapian path at 7c's start (X1); Q2 the WAL stays until 7b; Q3 http drops its offload path entirely; Q4 undecided, blobs stay in `DataStorage` with I/O in chores; Q5 T6 gates at Phase 6; Q6 read-your-writes stays the default.
- X1 keeps DataStorage; deletion of Xapian code is by git rm.
- Detent keeps its http pin until its move off http's offload path; 7c does not wait on it.
- Risk low: design and review only; no build or behavior change (the lead's mapping).

## May land early

None.
