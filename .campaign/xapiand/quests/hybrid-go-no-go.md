# hybrid-go-no-go: The hybrid-search go/no-go run

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hybrid-go-no-go`.

## Outcome

The owner decides, on measurements, whether strata goes for AI retrieval: C1 is rerun with strata
included at 1,000-vector tenants, then the experiments that show whether optimization can open a
clear lead in idle cost and p99 over pgvector, Qdrant, LanceDB and Elasticsearch. The outcome is go
(hybrid steps 1 to 4 of `docs/design/hybrid-search.md`) or dropping "best at AI retrieval" for good
(owner, 2026-10-28, D8; road-to-release "The strata lane" 3; strata `docs/status/cold-tier.md`).
Repository: Kronuz/strata.

## Acceptance criteria

- A1 C1 rerun with strata included at 1,000-vector tenants: idle cost and p99 against pgvector, Qdrant, LanceDB and Elasticsearch, per tenant count, under `exclusive` jobs on master code (D8): jobs
- A2 The optimization experiments that bound how far strata can lead on idle cost and p99, each a before and after at named commits (D8): jobs
- A3 [artifact] The results and the fit verdict updated from them, in strata's docs: `docs/design/hybrid-search.md`
- A4 [decision] The owner's go or no-go (hybrid steps 1 to 4, or "best at AI retrieval" dropped), and the owner's Q1 to Q6 of the design: a Decisions row

## Contracts

None until go; on go, the design's format kinds 23 to 26 and bit 17 become a format quest of their own.

## Decisions

- On go, the design's preliminary review (the architect: a format and cross-library change, kinds 23 to 26 and bit 17) belongs to the format quest that follows, not to this run.
- Parked by the owner (Q1 "not yet", 2026-10-27): resumes when the cold tier lands (`--needs
  cold-tier-step2`; status blocked until then).
- Adopts `design/hybrid` (strata:8c539a5, revision 0 and the owner's Q1 ruling); its documents land
  in strata's `docs/`.
- Machines: no AWS (D24). Running pgvector, Qdrant, LanceDB and Elasticsearch locally is this
  quest's setup; a system that cannot run on the host is reported as not measured.

## May land early

The design documents.
