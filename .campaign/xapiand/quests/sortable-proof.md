# sortable-proof: The sortable codec's remaining proof

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status sortable-proof`.

## Outcome

`Kronuz/sortable`, the MIT codec Xapiand adopts for every number it sorts, ranges or looks up by
term, carries the proof a decoder of users' disks needs: the exhaustive binary32 round trip, a
CPU-hour of decoder fuzzing, and a command-line tool the Python port's cross-language test drives
(`docs/design/sortable-encoding.md` on `design/sortable-encoding`, "First steps" 1). Repository:
Kronuz/sortable.

## Acceptance criteria

- A1 T2: `decode(encode(x)) == x` on every one of the 2^32 binary32 values widened, 0 failures (sortable-encoding T2): `cmake -DSORTABLE_EXHAUSTIVE=ON` then `sortable_exhaustive_float`, a `light` job
- A2 T4: one CPU-hour of `sortable_fuzz -max_total_time=3600 -timeout=5`, 0 crashes, 0 accepted inputs that fail to re-encode to themselves, ASan and UBSan clean (sortable-encoding T4): a `light` job
- A3 `tools/cli.cc` encodes and decodes for T3's harness (sortable-encoding first step 1): a test in the `build-test` check
- A4 T1 and T2's CI part pass on every CI platform and sanitizer job (sortable-encoding T10): sortable's CI and `cpp-libraries/tools/preflight.sh`

## Contracts

Provides the codec's bytes as of sortable:67b2a90, unchanged: `encode`/`decode`, 1 to 10 bytes,
`memcmp` order equal to numeric order across `int64`, `uint64` and `double`. A byte change here
voids `sortable-encoding`'s design.

## Decisions

- Review: the security reviewer's decoder-safety review (the design: "the floor for a decoder on
  users' disks").
- Prior evidence (audit, 2026-11-08), from sortable `docs/status/sortable.md` (history quest
  `sortable-step1`, sortable:33ed972), so it is not redone: T1/T2 property 50.9M values and T4 exhaustive
  83.9M short inputs, 0 failures; 38 mutants, 0 unexpected; ASan+UBSan and TSan 4 of 4; `fuzz/decode_fuzz.cc`
  4 workers x 10 minutes (279M runs, 0 findings: 40 CPU-minutes, short of A2's hour); CI on every platform.
  The ledger's Next 1 and 2 are this quest's A1 and A2.
- Starts now; it does not wait for the owner's Q1 (the design: step 1 "starts now").
- Open (lead): the repository's paths come from the design's citations (`test/property.cc`,
  `fuzz/decode_fuzz.cc`, `tools/cli.cc`); the checkout `../sortable` must exist beside the home.

## May land early

Tests and the fuzz corpus.
