# prospect-word-sum-memory: Scatter sums on the stack, not in every postings iterator

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at prospect:9de01a1 (2026-10-26); its ledger `docs/status/word-sum-memory.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-word-sum-memory`.

## Outcome

`postings_iterator` no longer carries the scatter's 1 KB of zeroed sums (CR3 of the count-union-bits review): `scatter` keeps 25 stack slots, so each iterator is 1 KB smaller and the timing is neutral.

## Acceptance criteria

Carried from the ledger's CR3 and Evidence, each shown by the ledger as an artifact.

- A1 [artifact] `sizeof(postings_iterator)` 4,424 to 3,400; malloc block 4,608 to 3,584; `fill_test` checks it: `docs/status/word-sum-memory.md`
- A2 [artifact] Dumps byte-identical and verify 0 mismatches on four indexes: `docs/status/word-sum-memory.md`
- A3 [artifact] Time neutral under `quiet strict` with an A/A (COUNT 0.998, TOP_10 1.004 against A/A 1.003): `docs/status/word-sum-memory.md`
- A4 [artifact] Revert check and 15 of 15 fill mutants caught; ASan+UBSan 39 suites clean: `docs/status/word-sum-memory.md`
- A5 [artifact] Linux, FreeBSD, arm64 timings: `docs/status/word-sum-memory.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Closes CR3 of `docs/reviews/count-union-bits.md`.
- Risk low: memory layout of an iterator, identity-checked (the auditor's mapping).

## May land early

None.
