# compact-slots: Compact buffer maps into eight-byte slots

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at strata:8fcd4e3 (2026-10-21); its ledger `docs/status/compact-slots.md` is the record. Its machine state is in `state.json`: `campaign status compact-slots`.

## Outcome

The write buffer's key-version and docid maps use 8-byte slots and grow at 7/8 load. The docid map compares stored docids, and the buffer flushes before a transaction exceeds its representable capacity.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Use 8-byte slots, 7/8 growth, docid comparison, and `buffered_docs_cap`: `docs/status/compact-slots.md`
- A2 [artifact] (no explicit criteria; from the Done list) Add named hashes and a spread test; add a limits.md row: `docs/status/compact-slots.md`
- A3 [artifact] (no explicit criteria; from the Done list) Erase shifts entries back without tombstones; test collisions, growth, wrap, rollback, and stale indexes: `docs/status/compact-slots.md`
- A4 [artifact] (no explicit criteria; from the Done list) Keep borrow/lease rules untouched; maps are not borrowed and clear with the buffer: `docs/status/compact-slots.md`
- A5 [artifact] (no explicit criteria; from the Done list) Add cap behavior: flush before transaction exceeds `buffered_docs_cap`; oversized single transaction is invalid: `docs/status/compact-slots.md`
- A6 [artifact] (no explicit criteria; from the Done list) Pass full Release, ASan+UBSan, and TSan ctest: `docs/status/compact-slots.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Buffer entries past 2^32 are refused by construction; default cap is 2^32 - 2 because 2^32 - 1 is the empty marker.
- Key map uses the high half of the 64-bit hash; xor-folding short-key hashes caused collisions.
- Risk normal: Changes ordinary buffer-map storage and growth behavior (the lead's mapping).

## May land early

None.
