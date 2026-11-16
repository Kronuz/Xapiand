# x-pool-step1: Add index pins before borrowed views

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:4f873c0e2 (2026-11-02); its ledger `docs/status/phase7c-x-pool.md` is the record. Its machine state is in `state.json`: `campaign status x-pool-step1`.

## Outcome

`IndexPin` holds today's reference count while an index is in use, and shard locks are awaited through `pin()`. The view check enforces pin-before-view ordering, and resident chores pin their endpoint without opening an index.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `docs/design/x-pool.md`: decisions, gate rows, and F1 to F6 rulings (revision 2, L2, XH8): `docs/status/phase7c-x-pool.md`
- A2 [artifact] Step 1: `IndexPin` over refs; `lock_shard` awaited at all 28 construction sites plus `dump_page`; pin-before-view check; autocommit resident pin: `docs/status/phase7c-x-pool.md`
- A3 [artifact] Step 1 proof on Linux: `docs/status/phase7c-x-pool.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Host-interface rulings recorded in `docs/design/x-pool.md`; pins precede views.
- Risk high: Changes concurrent index lifetime and view ordering (the lead's mapping).

## May land early

None.
