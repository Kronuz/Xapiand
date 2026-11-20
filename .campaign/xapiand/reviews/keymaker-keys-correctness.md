# keymaker-keys review: correctness

Commit: Xapiand:d9df2bbdaa214dbd3a732150430b4751983ad44e
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261119-165353-c9efd0 (coverage-multivalue test, pass), 261119-165403-4deb85 (suite, pass) and
  261119-165358-64761d (all four fixes reverted, fails) at Xapiand:78653b157. The merge to
  Xapiand:d9df2bbda touches no file under src/, tests/, cmake/ or CMakeLists.txt
  (`git diff --stat 78653b157 d9df2bbda -- src tests CMakeLists.txt cmake` is empty), so they stand
  for the reviewed commit. My probe at d9df2bbda passes the suite's 433 checks as well.
- The mutant log shows which checks fail without the fixes: lines 234-235 (M1 pins), 480 and 488
  (M5), 502-503, 508 and 515-517 (M4 and Jaccard). The descending-order checks the quest added for
  A1 (test_coverage_multivalue.cc:246-247) are not among them.

## What I checked myself

Probe job 261119-192902-57d2f7 at d9df2bbda; the scripts are in
`.scratch/xapiand/review-keymaker-keys-correctness/` (`run.sh`, `inject.py`, `probe.inc`). It
injects a probe into the coverage-multivalue test (two include lines move the suite's line numbers
by +2) and runs three variants: A, the quest code; B, only M1 reverted; C, only the Jaccard rebuild
reverted.

1. Round trip for every metric (Q1). All 12 string kinds × icase on and off × both directions ×
   alone and joined with a second key, mixed-case reference and values, five documents (one
   missing), serialized twice and cloned: 96 of 96 cases give equal bytes and equal keys (A). By
   reading the pinned libraries (string-similarity a5f50a6, soundex 3fd0402, double-metaphone
   b3191c4): Jaccard's `_set_str` is the only state derived in a constructor. Levenshtein and
   Jaro-Winkler restore all their fields one by one. The encoders hold only `_code_str`. The
   rebuild reapplies `strings::inplace_upper`, which folds ASCII only (strings 6799a6e) and is
   idempotent. With the Jaccard rebuild reverted (C), the suite's own checks fail (503 and 508 by
   the original numbering) and so do 52 probe cases. The Jaccard fix has its own failing test.
2. Bytes stored by released code (Q2). No production code calls `Multi_MultiValueKeyMaker::serialize`
   or `unserialize`: `grep -rn --exclude-dir=xapian` finds them only in keymaker.cc and the
   interface in engine/callbacks.h. handler.cc:1267-1365 builds the sorter from the query and lends
   it to the engine. Sort keys from `key()` exist only within a single search. Nothing is stored on
   disk. Persisted-format changes, all wire-only: (a) every string kind's slot data now starts with
   `serialize_length(slot) serialize_length(reverse)`, for example Jaro at slot 9 ascending
   `044a61726f 06 30 04 62657461` becomes `044a61726f 08 09 00 30 04 62657461`. Bytes in the old
   layout are refused with `engine::Error(invalid)` for all 12 kinds, never misread (PROBE-OLD in A).
   Released alpha.10 has the same pre-fix layout. (b) A phonetic key now reserializes to its
   original bytes (13 bytes, not 8, for soundex english). (c) A descending SerializeKey on a missing
   slot keys `ff00ffff` instead of `ffff` (F1).
3. Coverage pins (Q3): see F1 to F3.
4. Cost (Q4): `unserialize` has no production caller, so it costs nothing per query or per document.
   If a caller is added, the rebuild costs one `std::set<char>` over the reference string per slot
   per call. `clone()` (copy constructors) and `key()` are unchanged.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | M1 is not a fix. The empty string was already the least byte string, so the old `MIN_STR_CMPVALUE` keyed a missing slot last in descending order. One NUL byte never orders it better and orders it worse in two cases. A slot whose list values are all empty (`list("","")`) used to tie with the missing slot and now sorts after it in descending order, so the missing slot is no longer the minimum. A slot whose largest value is a NUL (`list("",nul)`) used to sort before the missing slot and now ties with it. Every realistic value orders the same either way: a, `boolean(false)`, `floating(-inf)`, `floating(-DBL_MAX)`, `integer(INT64_MIN)`, `positive(0)`, `timestamp(-1e15)`. A1 claims "a test checks the order and fails without the fix", but the order checks at test_coverage_multivalue.cc:246-247 pass with M1 reverted. Only the pins at 234-235 fail. | 261119-192902-57d2f7, PROBE-M1 rows in A against B. Variant B fails only at 236-237 (234-235 by the original numbering). Mutant 261119-165358-64761d lists no failure at 246-247 | The owner chooses. Recommended: restore an empty `MIN_STR_CMPVALUE` (spelled `std::string()`, commented as the least byte string), pin `biggest(none) == ""`, and add a descending-order check of `list("", "\0")` against a missing slot, which fails with the NUL constant. Alternatively, keep the NUL and record the two regressed cases as accepted. In both cases, correct A1's claim and the hunt row M1 |
| F2 | no | nit | The M5 and M4 pin flips are right. The `CHECK_NE` on the rebuilt key becomes `CHECK_EQ` (480), the phonetic size check becomes byte equality (515-517), and both fail without their fixes in the mutant | 261119-165358-64761d | none |
| F3 | no | gap | The suite's round-trip test (`every_kind`) covers icase only for Levenshtein and Sorensen_Dice and covers each kind in one direction. My probe covers the full product and passes. Adding a loop like it to the suite would protect the Jaccard rebuild when icase is on | 261119-192902-57d2f7 PROBE-RT, 96/96 | Optional: fold the icase × direction loop into test_round_trip_every_kind |
| F4 | no | nit | The changelog line says sorters written before this "no longer unserialize". That is accurate, and the bytes are refused with an engine error. No such bytes exist outside a process, because no production code serializes a sorter | PROBE-OLD rows; grep above | none |
| F5 | no | nit | Outside the quest's scope, for the hunt. StringList reads any single value whose first byte is 0x00 as a list (`SERIALIZED_LIST_MAGIC`), so a raw single value `"\0"` decodes as an empty list. Also, query_dsl.cc:1488 sorts a boolean field with `add_date` | PROBE-M1 `nul` rows; src/query_dsl.cc:1488 | File hunt rows if wanted |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit. A
finding that would remove or narrow user-visible behavior offers options, including keeping it,
and is never blocking.

## Not run

- CI at d9df2bbda was still running when I reviewed it (FreeBSD, arm64, Linux). A4 needs it green.
- No sanitizer or Debug build. No timing measurement of `unserialize`, because it has no
  production caller.
- I did not review the quest's other changes beyond src/metric_serialize.h, src/multivalue/keymaker.h
  and tests/test_coverage_multivalue.cc. The quest touches no other src or tests files.
