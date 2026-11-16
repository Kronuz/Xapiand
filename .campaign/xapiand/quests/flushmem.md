# flushmem: Streamed flush memory reductions

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:965339c (2026-10-26); its ledger `docs/status/flushmem.md` is the record. Its machine state is in `state.json`: `campaign status flushmem`.

## Outcome

Fusefilter gains a vector-consuming filter build, and termdict streams keys through a block sink. A streamed merge frees versions before filter construction; measured segment bytes and throughput remain unchanged.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `filter8::build(vector&&)` is added and filter-build bytes are 32 n + 1 MiB: `docs/status/flushmem.md`
- A2 [artifact] termdict builder accepts a block sink; merge and flush stream the keys section: `docs/status/flushmem.md`
- A3 [artifact] A streamed merge frees versions before filter construction: `docs/status/flushmem.md`
- A4 [artifact] Waiting and serializing measure as no help; smaller merges are argued, not built: `docs/status/flushmem.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Waiting and serializing are not adopted because measurements show no help; smaller merges are argued, not built.
- Risk low: Performance and memory measurements; no format or behavior change reported (the lead's mapping).

## May land early

None.
