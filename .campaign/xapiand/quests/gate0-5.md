# gate0-5: Gate 0.5, Xapiand on the engine seam

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:5b865a059 (2026-10-27), the lead's verdict "gate 0.5 passes" on run 2. The gate (step X5 of `docs/design/xapiand-on-strata.md` section 4) had no ledger: `docs/gates/phase0.5.md` is the record. Its machine state is in `state.json`: `campaign status gate0-5`.

## Outcome

Every Xapian call in Xapiand goes through the engine seam, with Xapian as its first implementation, and the seam changes nothing users see: the docs E2E has zero assertion regressions against the pre-seam build, response bodies differ only where explained, bulk index and query throughput stay within the noise band, the cluster and WAL checks pass unchanged, and each document carries exactly one key term.

## Acceptance criteria

Carried from strata `docs/PLAN.md`, "Phase 0.5" (its gate) and the rows T1 to T5 of `docs/design/xapiand-on-strata.md` section 5, each shown by the gate report as an artifact.

- A1 [artifact] T1 census: per E2E request, the facilities used, committed as `docs/dev/engine-census.md`: `docs/gates/phase0.5.md`
- A2 [artifact] T2 seam closed: zero files outside the allowlist contain `Xapian::` or a `xapian` include (`harness/seam_check.sh` exits 0): `docs/gates/phase0.5.md`
- A3 [artifact] T3 parity: zero E2E assertion regressions against a baseline captured at the branch point; response bodies differ only in volatile fields, or explained: `docs/gates/phase0.5.md`
- A4 [artifact] T3 and PLAN's gate: harness throughput (bulk index and query) within the noise band: `docs/gates/phase0.5.md`
- A5 [artifact] T4 cluster and WAL: `cluster_check.sh 2` and `3` and `wal_recovery_check.sh` pass unchanged on the Xapian implementation: `docs/gates/phase0.5.md`
- A6 [artifact] T5 key prefix: at most one term per document leads with the key prefix, and it is the id term: `docs/gates/phase0.5.md`
- A7 [artifact] The gate's extraction-ledger rows, X2's first among them (the version dances collapsed into one engine call): `docs/dev/extraction-ledger.md`

## Contracts

None recorded: landed work; the engine seam is in `src/engine/` and `docs/design/xapiand-on-strata.md`.

## Decisions

- Run 1 on Xapiand:cca0011bb failed T1 (no census build), T3 bodies (an unexplained estimate) and T3 throughput (bulk index 0.81x); the lead's verdict held the gate and ordered a bisect over the 19 first-parent commits.
- Run 2 on Xapiand:7efc3753e passed every row after the census (`census`, `seam-census`), the replay commit (`cr1-fix`), exact numeric ranges (`numeric-range`) and exact cluster aggregations; the bank estimate 459 became 232, the true count.
- Risk high: user-visible results, the WAL and the cluster on the new seam (this audit's mapping).

## May land early

None.
