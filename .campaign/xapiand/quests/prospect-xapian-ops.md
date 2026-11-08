# prospect-xapian-ops: Xapian-compatible operators (P3)

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:7f1d915 (2026-10-27); its ledger `docs/status/xapian-ops.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-xapian-ops`.

## Outcome

prospect gains SYNONYM, XOR, MAX and ELITE_SET scorers and a scorer over a host value source, matched to Xapian's recorded vectors, scoped by Xapiand's engine census and DSL (xapiand-on-strata P3); SCALE_WEIGHT and composite cases are deferred with evidence.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] O1 oracle parity per operator: match count and top-50 weights of Xapian's rows (vectors in `test/fixtures/xapian-ops/`): `docs/status/xapian-ops.md`
- A2 [artifact] O2 property tests of random operator trees against a brute-force model: `docs/status/xapian-ops.md`
- A3 [artifact] O3 a mutant per operator rule (`test/ops_mutants.py`): `docs/status/xapian-ops.md`
- A4 [artifact] O4 ASan+UBSan and TSan in their own build directories: `docs/status/xapian-ops.md`
- A5 [artifact] O5 CI green on the head (run 40616608446): `docs/status/xapian-ops.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Deferred with evidence: SYNONYM over non-term children, ELITE_SET keys for composite children, match estimates (P4), cursor-style sources, the values reader adapter (P1).
- The vectors come from `strata-oracle:6750aef` (branch `ops-vectors`), a laptop-only repository with no GitHub remote.
- Risk normal: query semantics checked against an oracle (the auditor's mapping).

## May land early

None.
