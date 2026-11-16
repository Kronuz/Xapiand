# phase0: Phase 0, baselines and evidence

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:a2d79a4a (2026-10-11), the commit that put the measured ingestion causes in `docs/FEASIBILITY.md`, which the Phase 1 review names as what closes Phase 0's gate. No ledger existed then: strata's `docs/PLAN.md` "Phase 0", `bench/phase0/README.md` (prospect's since the repository split) and `docs/reviews/phase-1.md` are the record. Its machine state is in `state.json`: `campaign status phase0`.

## Outcome

Baselines for Tantivy 0.26, Lucene 10.4.0 and Xapian 2.1.0 on the 5M-document Wikipedia corpus are recorded, with the VM and its fsync latency, and Xapiand's ingestion gap is attributed: Xapian is 28.2% of a RESTORE, the host's per-document synchronization most of the rest. The offload hop and the light-query p99 under heavy load are measured on current Xapiand, the Phase 6 baseline. The numbers are provisional (a shared host); the Phase 2 gate re-ran the engine rows on a quiet host.

## Acceptance criteria

Carried from strata `docs/PLAN.md`, "Phase 0: baselines and evidence": each item, then the gate. The plan has no ticks, so a row is met where the gate's record shows it.

- A1 [artifact] strata's Linux QEMU virtual machine (8 vCPUs, 16 GB): `tools/vm/linux.sh`
- A2 [artifact] strata's FreeBSD QEMU virtual machine: `tools/vm/freebsd.sh`
- A3 [artifact] `benchkit`'s interleaved-trial ratio runner: its code on main
- A4 [artifact] Benchmark harnesses for Xapian 2.x (direct, and driven Xapiand's way), Lucene and Tantivy on Wikipedia and on the Xapiand-shaped corpus: `bench/phase0/README.md`
- A5 [artifact] The Xapiand and Elasticsearch system-level harness with concurrent clients: `bench/phase0/README.md`
- A6 [artifact] A flame-graph profile of Xapiand's RESTORE and query paths, attributing time to Xapian calls against host code: `bench/phase0/README.md`
- A7 [artifact] The offload hop cost on the current build, the same query inline and offloaded: `bench/phase0/README.md`
- A8 [artifact] Light-query p99 under heavy load on current Xapiand (the Phase 6 baseline): `bench/phase0/README.md`
- A9 [artifact] The VM configuration and a measured in-VM fsync latency, recorded with every baseline: `bench/phase0/README.md`
- A10 [artifact] A dump of Lucene's FOR and PFOR postings on Wikipedia for `bitpack`'s size target: `bench/phase0/postings.cc`
- A11 [artifact] A GPL-side oracle harness, outside the strata repository, recording Xapian vectors (`TermGenerator` output, `QueryParser` descriptions, stemmer vocabularies, BM25 weights, `_percent`, estimates): `docs/reviews/phase-1.md`
- A12 [artifact] Gate: reproducible baseline JSON for every system: `bench/phase0/query.json`
- A13 [artifact] Gate: the profile's attribution of the ingestion gap: `docs/FEASIBILITY.md`
- A14 [artifact] Gate: the measured hop cost: `bench/phase0/README.md`

## Contracts

None recorded: landed work; the baselines and harnesses are in prospect's `bench/phase0/` and the documents the journal's Result names.

## Decisions

- Stage 1 review (strata `docs/reviews/stage-1.md`, 2026-10-11): Phase 0 starts after R1 to R3; R3 adds the RESTORE profile and the hop measurement.
- Phase 1 review, "Phase 0 open items" and "Phase 0 additions" (2026-10-11): the oracle vectors block only Phase 2's analyzer and parity rows; recording the RESTORE result in FEASIBILITY closes Phase 0's gate; the hop cost and the light-query p99 are VM numbers, re-measured on bare metal before they are quoted, and the Phase 6 baseline is taken there.
- Risk low: measurements and harnesses, no product code (this audit's mapping).

## May land early

None.
