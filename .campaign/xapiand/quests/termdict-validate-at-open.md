# termdict-validate-at-open: termdict validates its index at open

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at termdict:8b1e964 (2026-10-26); its ledger `docs/status/validate-at-open.md` in Kronuz/termdict is the record. The quest's repository is `../termdict`. Its machine state is in `state.json`: `campaign status termdict-validate-at-open`.

## Outcome

`reader::open_index` validates the FST and maps a malformed index to `termdict::corrupt` instead of `std::invalid_argument`, with an index-mutating sweep and fuzz target; blocks stay lazily validated.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 `open_rejects` hand-built FSTs per rejected case and `sweep_index` over four dictionaries; the reverted fix fails: `docs/status/validate-at-open.md`
- A2 [artifact] K2 `fuzz/fuzz_open.cc` 13,615,556 runs in 2,101 s, no report; corpus of 675 files committed: `docs/status/validate-at-open.md`
- A3 [artifact] K3 mutants killed; the survivors are subsumed by another check: `docs/status/validate-at-open.md`
- A4 [artifact] K4 open cost on the 3,964,752-term Wikipedia vocabulary: about 5.8 ms and 3 MB transient, unchanged: `docs/status/validate-at-open.md`
- A5 [artifact] K5 CI on the pushed branch: `docs/status/validate-at-open.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Cheaper validation belongs to the fst repository (not done).
- Risk high: a decoder of on-disk dictionaries (the auditor's mapping).

## May land early

None.
