# strict-text review: correctness

Commit: 7376bd7a3381 (code at 44b906765a98; the commits after it change only the journal)
Verdict: APPROVE
Date: 2026-11-16

## Evidence I relied on

- 261116-152435-db3a1a: at `src` of master, `test_strict_text` fails (16 of 40 checks), so both new tests fail without the change. I accept it as the fails-without evidence for A1 and A2.
- 261116-152427-5d1269: the build plus `test_strict_text`, `test_node`, `test_serialize` and `test_aggregation_shards` pass at 44b906765. It is evidence for the existing strict cases, but it is not the suite (see S1).

## What I checked myself

Probe job 261116-154654-e8ba79 (`.scratch/review-strict-text-correctness/run.sh` and `test_probe.cc`; it copies the test into the job checkout and builds it). For about 70 texts it prints, per type (integer, positive, floating), the text result, the result of the same number parsed by the real JSON loader, the serialized bytes, and the three casts.

1. Grammar (Q1). `json_number_text` is the JSON number grammar exactly, and every typed path calls `Serialize::exact` (writes at schema.cc:5356, `_id` at schema.cc:2891/2900, terms at query_dsl.cc:689, range bounds at query_dsl.cc:175 and 883, `_term` filter at bucket.cc:70/74). Refused with a 400 naming field and value: `0x10`, `-0x10`, `+1`, `012`, `-012`, `00`, `00.5`, ` 1`, `1 `, `\t1`, `1\n`, `1.`, `.5`, `1e`, `1e+`, `--1`, `1-`, `1.2.3`, `1e2e3`, `-.5`, NUL inside or after the digits, non-ASCII digits, `inf`, `nan`, empty, `-`. Accepted and equal in bytes to the JSON number: `0`, `-0`, `0.0`, `-0.0`, `0e0`, `1e3`, `1E3`, `1e+3`, `12.0` (integer 12), `1.5e2`. Casts are untouched (`cast.cc` is not in the diff): `{"_integer":"012"}` still reads 12 and `{"_float":"0x10"}` still reads 16, which is D92's explicit-cast door.
2. `_term` filter (Q2), through the real `AggregationMatchSpy` constructor on integer, positive, floating and keyword fields. Integer `age`: `12.0`, `12`, `"12"`, `"12.0"`, `-1` and `[1,2.0]` accepted; `12.5` answers `400 Field 'age' expects a value of type integer, got 12.5`; `[1,2.5]` names `'age'` and `2.5`; `"0x10"`, `"012"`, `true`, `null`, `"abc"`, 2^53 as a float and `1e30` are refused with the field named. Positive `h` refuses `-1`. Floating `w` accepts 12.5. A cast with a bad body names the field and the cast error. The field is named on every refusal.
3. Wrap, truncation and silent accept (Q3). Every row below is the same as the JSON number, and a refusal names field, value and type.
   - `-0`, `-0.0` and `-0e0` read as 0 (floating keeps `-0.0` for the float forms; `-0` is the int 0, as the JSON loader reads it).
   - 2^53: `9007199254740992` and `9007199254740993` are exact integers. `9007199254740992.0`, `9007199254740993.0` and `9007199254740993.5` are refused on integer and positive, and read as the nearest double on floating.
   - 2^63: `9223372036854775807` is exact. `9223372036854775808` is refused on integer and is a uint64 on positive. `-9223372036854775808` is exact on integer. `-9223372036854775809` is refused on integer and positive.
   - 2^64: `18446744073709551615` is exact on positive and refused on integer. `18446744073709551616` is refused on integer and positive. `-18446744073709551615` is refused on integer and positive (no wrap).
   - Long digit strings: 30 digits, `1`+400 zeros, 5000 nines, `1e400` and `1e99999999999999999999` are refused on integer and positive. `1e308` is refused on integer and positive and accepted on floating.
4. Leading zeros (Q1, last part). Nothing in the suite, harness or docs reads a leading-zero or `+` text on a numeric field. The `"015"`/`"00100"` strings in test_engine_query.cc and test_engine_callbacks.cc are engine-level terms, not typed values. String values never become numeric fields by detection (`Schema::guess_concrete_type` does not guess numbers from text), so the `postcode: "05192"` rows of docs/public/assets/accounts.ndjson stay text. The refusal is the only user-visible narrowing: a numeric `_id` of `007` or `q=age:012` now answers 400.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| S1 | no | gap | The lead's gates are open: A1 and A2 show no passing job tagged for them (only the fails-without jobs); the check `build-test` job 261116-153459-d05e50 failed with exit 127 (`campaign:build-test: command not found`, a tooling error, not a test failure); 261116-152355-9a55ab failed because its build directory did not exist. The only passing job runs four test binaries, not the suite. CI at 7376bd7a3 was still running when I looked. | `campaign status strict-text`; `campaign job log 261116-153459-d05e50` | Lead: resubmit the passing job `--for A1,A2` (261116-152427-5d1269's command at the head) and rerun `campaign verify strict-text`; land on CI green. |
| S2 | no | nit | Underflow differs from the JSON loader on floating (and on integer/positive): text `1e-400`, `1e-99999999999999999999`, `4.9e-324` and `0.`+400 zeros+`1` answer 400 (strict_stod reports ERANGE), where a JSON number reads 0 or the minimum subnormal. It refuses, never wraps or mislabels, so it is the safe direction. | probe job 261116-154654-e8ba79, lines marked DIFFERS | Keep (a refusal is stricter), or document that text which underflows is refused. |
| S3 | no | nit | JSON bodies accept the `Infinity`, `-Infinity` and `NaN` literals (relaxed parser flags). The text `"Infinity"` and `"-Infinity"` is refused on floating. This is "JSON number grammar exactly" for text, but the two spellings differ. | probe job 261116-154654-e8ba79 | Keep; note it with S2 in the docs. |
| S4 | no | nit | `numeric-type.md` (Exact Values) says text that spells a number is the number (`"12"`, not `" 12"`), but not that hex, `+`, leading zeros, `Infinity` and underflow are refused. The leading-zero refusal is user-visible, so the page should state it. | docs/src/content/docs/reference-guide/schemas/field-types/numeric-type.md:15-18 | Add one sentence naming the JSON grammar. |
| S5 | no | nit | `test_strict_text` pins the grammar rows and 12.0/12.5, but not the boundaries I probed (`-0`, 2^53, 2^63, 2^64, exponent forms, a long digit string). They hold today. | tests/test_strict_text.cc | Add the probe table's boundary rows as CHECKs, then rerun probe job 261116-154654-e8ba79's script to confirm nothing moves. |
| S6 | no | nit | The `_term` filter on a keyword or text field now refuses a JSON number (`12` answers 400) as a search does, because `Serialize::exact` requires text there. It follows D92 and the search path, so it is consistent, but it is a narrowing of the filter; the owner can keep it or read numbers as text only in `_term`. | probe job 261116-154654-e8ba79, `TERM k` line | Keep (one rule everywhere); note it in the changelog with C5/C6. |

## Not run

- The full suite and the e2e harness. I relied on job 5d1269 for four binaries only; S1 asks for the suite at the head.
- A running server: the 400 status and body for `_term` come from catching `ClientError` in the `AggregationMatchSpy` constructor, not from an HTTP request.
- Linux or FreeBSD (CI's), and any Release or sanitizer build; the probe ran RelWithDebInfo with asserts on, macOS arm64.
- Locale behavior: strict_stox reads `.` whatever the locale says; I did not vary it.
