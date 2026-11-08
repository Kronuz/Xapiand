# t11-barriers: Commit barrier ordering experiments

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:7125dfc28 (2026-10-29); its ledger `docs/status/t11-barriers.md` is the record. Its machine state is in `state.json`: `campaign status t11-barriers`.

## Outcome

Commit ordering batches filesystem operations around a barrier, with explicit error handling and an opt-in full-flush option. The barrier orders writes but is not itself a flush; the ledger reports no performance experiment landed.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] E1 Fold first directory barrier into data batch: `docs/status/t11-barriers.md`
- A2 [artifact] E2 Order manifest temporary with a barrier: `docs/status/t11-barriers.md`
- A3 [artifact] E3 Use one ordering point and one flush: `docs/status/t11-barriers.md`
- A4 [artifact] Add strata faultfs ordering-point models: `docs/status/t11-barriers.md`
- A5 [artifact] Add cleanup, docs, and dependency pins: `docs/status/t11-barriers.md`
- A6 [artifact] Fix TSan race in segment reference copy: `docs/status/t11-barriers.md`
- A7 [artifact] L1 CB-1: fall back to `F_FULLFSYNC` only on listed unsupported errors; other errors return: `docs/status/t11-barriers.md`
- A8 [artifact] L2 CB-2: route PosixIO sync calls through replaceable sync calls and test exact sequences: `docs/status/t11-barriers.md`
- A9 [artifact] C1 CB-3: docs state barrier orders, is not flush, guaranteed only on Apple SSDs; add opt-in `full_flush_ordering`: `docs/status/t11-barriers.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Architect's scoped check rejected CB-1 and CB-2; the fixes landed here and the re-check (2026-10-29,
  strata `docs/reviews/commit-barriers-durability.md`) closed L1, L2 and C1; C2 and C3 are carried by
  `barrier-warning-strata` A5, A6.
- EINVAL is not treated as an unsupported barrier error.
- Risk high: changes durability ordering and sync behavior (the lead's mapping).

## May land early

None.
