# prospect-f13-gate: Format 13 confirmation rows on the gate VM

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at prospect:aa532de (2026-10-19); its ledger `docs/status/f13-gate.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-f13-gate`.

## Outcome

Format 13 with the query items (main 8110f1d) is confirmed at 5M on the gate VM before the freeze: identity, bytes, merge times, run-level queries against the baselines, and the owed rows, with D15's postings constraint and T5's indexing guard reported as missed.

## Acceptance criteria

No explicit criteria; from the task's rows, each shown by the ledger as an artifact.

- A1 [artifact] Row 1 (C1, FQ1, FQ2): dumps identical A against B on both layouts, 1,162 queries; verify 0 mismatches: `docs/status/f13-gate.md`
- A2 [artifact] Row 2 (D15): postings within keyed `-bp`'s `.doc`: `docs/status/f13-gate.md`
- A3 [artifact] Row 3 (C3, FQ4): merge times and indexing within T5's 1.03 of format 12: `docs/status/f13-gate.md`
- A4 [artifact] Row 4a: B against A at run level, four sessions, 0 losses: `docs/status/f13-gate.md`
- A5 [artifact] Row 4b: the harness against Lucene, Lucene -bp, Tantivy and PISA on S': `docs/status/f13-gate.md`
- A6 [artifact] Row 5: merged terms against format 12, 16 and 15 wins, 0 losses; serving memory equal (581 MB): `docs/status/f13-gate.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- For the lead: whether D15's 9.96 MB miss is carried or blocks the freeze (open).
- The freeze decisions (D15's carry, C2's ruling on FQ3) are the lead's and the architect's.
- Risk low: measurement only; no code (the auditor's mapping).

## May land early

None.
