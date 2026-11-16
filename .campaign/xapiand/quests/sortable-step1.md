# sortable-step1: The sortable codec library (step 1)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at sortable:33ed972 (2026-11-01); its ledger `docs/status/sortable.md` in Kronuz/sortable is the record. The quest's repository is `../sortable`. Its machine state is in `state.json`: `campaign status sortable-step1`.

## Outcome

Kronuz/sortable is published: `sortable.hh` (D2-D5, D7), constexpr-evaluable, its bytes fixed by `tools/model.py`, with property, size, mutant, sanitizer and fuzz evidence and CI on every platform (sortable-codec step 1).

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `sortable.hh`, tests, fixtures from `tools/model.py` (113,491 vectors, 39 reject witnesses), README, AGENTS, `docs/mechanisms/sortable.md`, CI workflow: `docs/status/sortable.md`
- A2 [artifact] T1/T2 property 50.9M values, 0 failures; T6 all vectors; T4 exhaustive 83.9M short inputs, 0 failures: `docs/status/sortable.md`
- A3 [artifact] T5 with R1: `test/size.cc` size sets: `docs/status/sortable.md`
- A4 [artifact] C4 and D2/D3 mutants: 38, 0 unexpected: `docs/status/sortable.md`
- A5 [artifact] ASan+UBSan and TSan, 4 of 4: `docs/status/sortable.md`
- A6 [artifact] Fuzz `fuzz/decode_fuzz.cc` 4 workers x 10 minutes, 279M runs, 0 findings: `docs/status/sortable.md`
- A7 [artifact] C1: 20 call forms compile and 7 do not, as `static_assert`s: `docs/status/sortable.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Its Next (T3 exhaustive float, a CPU-hour of fuzzing, decoder and code reviews) is the live quest `sortable-proof`.
- Risk high: a decoder of users' disks; bytes frozen by the model (the auditor's mapping).

## May land early

None.
