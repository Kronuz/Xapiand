# prospect-phase7c-l4: Sliced search (L4)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at prospect:c77d6eb (2026-10-30); its ledger `docs/status/phase7c-l4.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-phase7c-l4`.

## Outcome

prospect's search yields at window boundaries: one `co_await slice.checkpoint()` mechanism in every scorer loop, between segments and between shards, with the pinned view owned by the search frame, cancellation at every checkpoint, and a measured no-yield cost.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] S1 identity: `sliced_test` against the synchronous search and the scoring model (4,536 random-corpus comparisons plus 2 x 567): `docs/status/phase7c-l4.md`
- A2 [artifact] S2 yields: at a 0 ns slice, >= 0.9 of the windows resume for each operator; counted OR and AND_NOT >= 1.8: `docs/status/phase7c-l4.md`
- A3 [artifact] S3 cancellation: the search ends at the next checkpoint with `task::cancelled`, no visitor call after, the view freed: `docs/status/phase7c-l4.md`
- A4 [artifact] S4 mutants: 17 in `test/sliced_mutants.py`, all die on the final code: `docs/status/phase7c-l4.md`
- A5 [artifact] S5 ASan+UBSan and TSan clean on the head prospect:5c29324 code: `docs/status/phase7c-l4.md`
- A6 [artifact] S6 no-yield overhead, `quiet strict`, 16 rounds x 5 runs: multi-term 1.014 to 1.022 against A/A 0.997 to 1.003: `docs/status/phase7c-l4.md`
- A7 [artifact] S7 preflight mac-hardened ok (the VM runs stopped); CI run 41054381629 green on all 8 jobs: `docs/status/phase7c-l4.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Design: Xapiand `docs/design/phase7c.md` D2, D12 and section 5 L4; the design review names no L4 finding.
- The residual 2% no-yield cost was left open; it landed later as prospect:5c98f81 (`perf/sliced-residual`, `docs/perf/sliced-search-residual.md`, 1.010 to 1.028), the Perf half `perf-watch-peek` names.
- Risk high: every query path now suspends and resumes across threads (the auditor's mapping).

## May land early

None.
