# prospect-values-bytes: Values plugin, opaque bytes slots (P1)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at prospect:eee749f (2026-10-27); its ledger `docs/status/values-bytes.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-values-bytes`.

## Outcome

prospect stores values as one docvalues `bytes` column per slot with a checked slot directory, through flush, merge, open, check and digest, read by `value(ord, slot)` and presence iteration (xapiand-on-strata P1, Q2 (a)).

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `bytes` column per slot, slot directory, flush, merge, open, check, digest record, reader, fixtures, pre-freeze bump (0x8000000B): `docs/status/values-bytes.md`
- A2 [artifact] A merge keeps every value of live documents and drops deleted ones, byte for byte against one flush: `docs/status/values-bytes.md`
- A3 [artifact] A revert check per bug fixed (eager open, TSan hang of `std::call_once`): `docs/status/values-bytes.md`
- A4 [artifact] Nine mutants (A to I) fail: `docs/status/values-bytes.md`
- A5 [artifact] Sanitizers: ASan+UBSan and TSan in their own trees: `docs/status/values-bytes.md`
- A6 [artifact] Decoder safety: `fuzz_values` 12,228 runs in 10 minutes, corpus of 275 replayed: `docs/status/values-bytes.md`
- A7 [artifact] One CPU-hour of fuzzing per decoder (the design): `docs/status/values-bytes.md`
- A8 [artifact] `docs/mechanisms/values.md`: `docs/status/values-bytes.md`
- A9 [artifact] CI green on every job (prospect:8a1b148; the last push queued): `docs/status/values-bytes.md`
- A10 [artifact] Screen A/B: indexing without values unchanged (1.197 against 1.184 s): `docs/status/values-bytes.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Ten layout decisions recorded in the ledger (column per slot, directory in the tail, structure bit 16 on by default, reserved slot 0xFFFFFFFF, empty value is a value, 64 MiB limit, merge by slot).
- Strata prerequisite strata:b644708 (rank 7 for values).
- Risk high: a new on-disk section and its decoder (the auditor's mapping).

## May land early

None.
