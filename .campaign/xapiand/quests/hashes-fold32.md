# hashes-fold32: wordwise hash32: the high half, not the xor-fold

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at hashes:975492a (2026-10-26); its ledger `docs/status/fold32.md` in Kronuz/hashes is the record. The quest's repository is `../hashes`. Its machine state is in `state.json`: `campaign status hashes-fold32`.

## Outcome

hashes adds `wordwise::hash32` and `wordwise_ci::hash32` (the high half of the 64-bit hash) after the xor-fold collided on short keys (strata's compact-slots), with every 32-bit output measured; no existing output changes.

## Acceptance criteria

Carried from the ledger's Done list, each shown by the ledger as an artifact.

- A1 [artifact] Every 32-bit output and reduction measured on exhaustive short-key sets (`bench/collisions.cc`; tables in `docs/BENCHMARKS.md`): `docs/status/fold32.md`
- A2 [artifact] `hash32` on `wordwise` and `wordwise_ci`, all overloads, constexpr: `docs/status/fold32.md`
- A3 [artifact] Test: 64-bit outputs pinned; the worst set (117,576 keys) 0 collisions against 116,575 for the xor-fold; two mutants caught; ASan+UBSan clean: `docs/status/fold32.md`
- A4 [artifact] CI: `docs/status/fold32.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Rollout advice for the lead: 32-bit consumers of wordwise call `hash32`; persistent callers keep their function unless they hit the collision.
- Risk low: an additive hash; persisted outputs unchanged (the auditor's mapping).

## May land early

None.
