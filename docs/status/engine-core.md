# engine-core (X2, phase 0.5)

Branch `phase0.5/engine-core` from Xapiand master `5cce4860d`. Design:
`strata/docs/design/xapiand-on-strata.md` sections 3 and 4 (row X2), gate rows T2 to T4.

## State

X2 is split at the design's seam. Half A (this launch, in progress): the seam's types, errors and
`Document`/`DocView`, the Xapian implementation, and every Document producer and consumer on the
write path (schema, generate_terms, handler prepare/index, shard writes, WAL). Half B (plan below):
`engine::Shard` lifecycle and write slice, errors in catches, pool, `exception_xapian.h`.

## Done criteria (design row X2)

`src/engine/` with types, errors, `Document`, `DocView`, the `Shard` write and lifecycle slice, Xapian
implementation; `schema.cc`, `generate_terms.cc` (document side), `handler.cc` prepare and index,
`shard.{h,cc}` writes, `wal.{h,cc}`, `pool`, `flags.h`, `exception_xapian.h` migrated; `Shard::db()`
still exists for reads; E2E and WAL recovery unchanged.

Proof owed: E2E 576/576 assertions and `e2e_diff` green against the X1 baseline; `harness/wal_recovery_check.sh`
passes; seam count before and after; extraction-ledger row.

## Done

(nothing committed yet)

## Next

## Blocked on

