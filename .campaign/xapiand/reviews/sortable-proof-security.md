# sortable-proof review: security

Commit: sortable:937c7c6782e99937be0eb6b74dea29c2cc23293d
Verdict: APPROVE
Date: 2026-11-09

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

Attacker: a malicious or corrupted file on a user's disk (a term or value slot read back into
`sortable::decode`). What they could gain: an out-of-bounds read, a hang, or two byte strings for
one value, which would split one number into two terms and break term lookup and range order.

## Evidence I relied on

- 261109-145954-894c13 (A2): I read the log. The binary was built with `-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all`, ran 363,143,404 runs in 3601 s with `-timeout=5`, ended `DONE cov: 312 ft: 663`, and logged no crash, abort or timeout. Inputs went up to 4096 bytes (`lim: 4096`). The seed corpus was 1,210 files.
- 261109-145953-7b39eb (A1): `checked 4294967296 floats`, `all floats passed`.
- 261109-145956-b3bf94 (A3): 5 of 5 CTest tests, including `sortable_cli`, which runs `check_cli.py`'s malformed-line cases (exit 2, no stdout).
- The A4 CI run 42717337992 and preflight 261109-161721-204fdf, as listed in the journal. I did not reread these: they are platform evidence, outside this lens.

## What I checked myself

Scripts are under `.scratch/review-sortable-proof-security/` in the home checkout.

1. **Fuzz oracle against mutants.** Job 261109-163356-a7173d (`fuzz_mutants.py`) built `fuzz/decode_fuzz.cc` (libFuzzer, ASan+UBSan) against each of the 38 mutants in `tools/mutants.py`, plus 4 of mine, and ran each for 60 s from `make_corpus.py`'s seeds. Result: 43 cases, 0 not as expected. The baseline survived 60 s.
   - Every D5 reject-rule mutant died on the oracle `accepted, not canonical`, `order` or `round trip`, not on a crash. Most died in 1-2 s. D5.9 "a double may reach 2^1024" took 60.5 s.
   - Out-of-bounds mutants died under ASan heap-buffer-overflow: D5.1 empty, D5.3 dropped, my `size < 2` and my `k <= len`. D5.6 alone died under UBSan on a shift exponent.
   - My hang mutant (the tail loop never ends on a 7-byte tail) died on libFuzzer's `-timeout=5`.
   - The conversion mutants died on `to_double not nearest`, `conversion order`, `saturate_uint64 not toward zero` and `round trip`.
   - Survivors were only the declared equivalents: D5.4 upper bound, D5.8 alone, and my negative bound of 1025. D5.9 rejects each of these.
2. **Hand-built `number`.** Job 261109-163400-fac723 (`number_ubsan.cc`) built 37,048,320 hand-made `number`s and called every conversion under ASan+UBSan with `-fno-sanitize-recover=all`. They covered all 256 `kind` bytes, both signs, exponents from INT_MIN to INT_MAX (every value in -1200..1200 plus extremes) and 30 significands, including ones with bit 63 clear. Exit 0, no report.
3. **Reading `sortable.hh:282-346`.**
   - Every `at(i)` is guarded: `size >= 1` first, then `size >= 3` before the field, and the tail loop is bounded by `size - i <= 8`.
   - The only loop is that bounded tail loop.
   - Escape exponents are clamped to [-1074, -61] and [64, 1024]; direct tags give [-60, 63].
   - After the negative shift, `number.exponent` stays within [-1074, 1023], or [0, 63] for an integer.
   - Canonicality is semantic: the fuzz oracle `exact::reencode(*n) != in` is a function of the decoded value. So two inputs that decode to one value cannot both pass, whatever the rule structure.
4. **Reading `tools/cli.cc` against risk 3.**
   - Hex parsing checks `size % 2` before `text[i + 1]`.
   - Numbers use `std::from_chars` with an end check, so it rejects `+`, spaces, `0x`, `\r` and NUL.
   - Negation of 2^63 is modular in C++20.
   - `malformed()` flushes and calls `std::exit(2)`. Lines before a malformed one have already been printed, but `check_cli.py` fails on any non-zero exit, so a malformed line cannot pass as an answer.
   - I found no path to undefined behavior.
5. **Xapiand reachability (risk 4).** `git grep` for `sortable.hh` and `sortable::` in Xapiand, excluding the GPL files, finds only campaign docs: Xapiand does not include the library yet.
   - In the design, disk bytes reach a `number` only through `decode`, which bounds it (item 3).
   - User input arrives as JSON `int64`, `uint64` or `double` and goes through `encode`, which is total on those types.
   - Nothing in the design builds a `number` from untrusted fields. Item 2 shows that even a hand-built `number` cannot reach undefined behavior.

Answers to the named risks:

1. **Out of bounds, loops, non-canonical input:** none found. The fuzz oracle detects each class, shown by a kill per mutant.
2. **Order and round trip:** `exhaustive_float.cc` proves both over every binary32. It walks the values in order, checks that each round trips bit for bit and that each encoding is strictly above the previous one, and asserts the count is 2^32.
   - The CLI-vs-model check proves byte equality with the model, the decoder's accept and reject decisions, and decode-then-encode round trip. It does not check order. For binary64, order comes from the model's own `check`, `sortable_property`, and the fuzz pair and cross-type order oracles.
   - The model is independent in value: `Fraction` arithmetic and a semantic test of whether a value is a double or an integer. It is not independent in structure: its decode applies the same D5 rule order. The re-encode oracles in both cover that shared shape.
3. **CLI protocol:** sound by reading and by `check_cli.py`'s malformed cases in the passing build-test job. My sanitizer probe of the CLI had not finished when this review was written (see Not run).
4. **Hand-built `number`:** not reachable from untrusted input in Xapiand's planned use, and free of undefined behavior even when built by hand.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| S1 | no | nit | Journal Rulings 5 and the mechanism note say an out-of-range `exponent` "can overflow `to_double`'s arithmetic". It does not: every conversion is free of undefined behavior for any `number`, because `-1022 - e` cannot overflow for any `int`, and every shift is bounded. | job 261109-163400-fac723; `sortable.hh:113-135` | Reword: a hand-built `number` converts without undefined behavior but may give values that no `decode` produces. |
| S2 | no | gap | The `sortable_cli` probe under ASan+UBSan, which checks every line's exit and answer against a reference parser and the model, hit its 20-minute deadline with no result: about 10,000 ASan process starts on a loaded host. A shorter rerun is queued. | job 261109-163358-a2c728 (timed out); rerun 261109-165856-4e24de (`PROBE_RANDOM=400`), `cli_probe.py` | The lead reads 261109-165856-4e24de. Expected: `cli probe: ok`. A failure there reopens risk 3. |
| S3 | no | nit | The fuzz canonicality oracle runs only on the whole input, and every seed starts with a split byte from 0 to 6, so most seeds are rejected as whole inputs. libFuzzer still reaches accepted inputs quickly (item 1), but D5.9's 2^1024 mutant took 60 s. | job 261109-163356-a7173d | Optional: also run `reencode` on the halves `a` and `b`, or add unprefixed seeds. |
| S4 | no | nit | The CLI accepts lenient forms: leading zeros (`i 007`), `i -0` and uppercase hex. They are harmless, because answers are compared byte for byte, but the protocol text does not mention them. | `tools/cli.cc:37-41,59-68` | Document them, or keep as is; the owner decides. |

## Not run

- The completed result of the CLI sanitizer probe (S2).
- Fuzzing on Linux or arm64; the fuzz hour and my mutants ran on macOS x86_64 with Homebrew clang 23.
- TSan: not relevant, since `decode` is pure.
- The Python port and T3 itself, which are step 3 of the design.
- I opened none of Xapiand's GPL sortable sources.
