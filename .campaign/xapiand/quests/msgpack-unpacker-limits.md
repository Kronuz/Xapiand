# msgpack-unpacker-limits: Unpacker reserves only what the bytes justify

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at msgpack:88aab56 (2026-11-03); its ledger `docs/status/unpacker-limits.md` in Kronuz/msgpack is the record. The quest's repository is `../msgpack`. Its machine state is in `state.json`: `campaign status msgpack-unpacker-limits`.

## Outcome

msgpack's streaming unpacker no longer reserves `n * sizeof(object)` for a hostile `array32` or `map32` header: storage grows with the bytes in hand, so five bytes cannot ask the zone for 96 GB, with the security review's SR1 to SR5 addressed.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Tests fail with the fix reverted (hostile headers one chunk, one shot, few bytes behind, nested, split, after valid items): `docs/status/unpacker-limits.md`
- A2 [artifact] 13 mutants (`tools/unpack_mutants.py`) die: `docs/status/unpacker-limits.md`
- A3 [artifact] ASan+UBSan 11 of 11 ctest targets: `docs/status/unpacker-limits.md`
- A4 [artifact] Fuzz `fuzz/unpacker_fuzz.cc` 300 s, 3,224,200 and 999,795 runs, no finding: `docs/status/unpacker-limits.md`
- A5 [artifact] Xapiand built against the branch: `restore_stream_check.sh` six rows ok, E2E GREEN, hostile RESTORE returns 200: `docs/status/unpacker-limits.md`
- A6 [artifact] Security review SR1 (fixed in the mechanism), SR3, SR4, SR5 folded in; SR2 is Xapiand `fix/restore-depth-limit`: `docs/status/unpacker-limits.md`
- A7 [artifact] The lead measures the unpacker on the corpus before landing (`msgpack_benchmark`): `docs/status/unpacker-limits.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Explicit limits like `msgpack_load` cannot work for a stream; the reservation is bounded by bytes in hand (Design vs code).
- The depth limit still defaults to 0xffffffff; callers parsing untrusted bytes pass one, as Xapiand does.
- Risk high: a decoder of untrusted request and RESTORE bytes (the auditor's mapping).

## May land early

None.
