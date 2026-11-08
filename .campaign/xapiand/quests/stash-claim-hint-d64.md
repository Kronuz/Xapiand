# stash-claim-hint-d64: Claim hint depth-64 fallback (attempt)

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at stash:3bfc455 (2026-11-01); its ledger `docs/status/claim-hint-d64.md` in Kronuz/stash is the record. The quest's repository is `../stash`. Its machine state is in `state.json`: `campaign status stash-claim-hint-d64`.

## Outcome

An attempt to bring `bench_steady`'s depth-64 p99 rows back to START after the claim hint: a fallback (a leaf's first eight chunks run START's code) landed and missed the bar; `stash-claim-hint-publish` settled it.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] depth-64 t=4 and t=14 p99 at least as fast as START on the M4 Pro and the i9: `docs/status/claim-hint-d64.md`
- A2 [artifact] No other row of the four benches more than 3% slower than HEAD: `docs/status/claim-hint-d64.md`
- A3 [artifact] The chain-walk win kept (`test_claim_walk`): `docs/status/claim-hint-d64.md`
- A4 [artifact] Concurrent tests clean under TSan; a depth-64 stress with 14 producers that fails on a lost or duplicated claim: `docs/status/claim-hint-d64.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Superseded by `stash-claim-hint-publish` (stash#1).
- Risk normal: claim ordering in a lock-free structure (the auditor's mapping).

## May land early

None.
