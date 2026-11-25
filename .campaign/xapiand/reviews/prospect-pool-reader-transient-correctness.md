# prospect-pool-reader-transient review: correctness

Commit: prospect:c99e6bed2854c560be6de5cf09797c4af1bcf34a
Verdict: APPROVE
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261124-160815-50fda7 (fielddict_test at prospect:c4e3e64, exit 0) and 261124-181031-a6c46d (the
  same at prospect:c99e6be, exit 0): the A1 test passes at the head. `include/**` is unchanged
  between c4e3e64 and c99e6be (`git diff --stat c4e3e64..c99e6be` touches only the test, the doc
  and the changelog).
- 261124-160817-d40a36 (mutant: check_fields scans through `pool()` again, `--fails-without`,
  exit 1): it fails on exactly the two heap checks after `check_fields`
  (test/fielddict_test.cc:445 and :447) and on nothing else. The mutant is the base's scan, so the
  verifier and "pooled field id" assertions passing on it also show those refusals equal base's.
- 261124-160820-8db0ed (build-test at c4e3e64, 59/59): the suite with the same `include/`.
- 261124-160830-97749f (A3 strict, base 44303965 / fix c4e3e64 / A/A): its log is the doc's table,
  number for number.
- CI run 45516118177 at prospect:e6cfcd5: every platform green but linux-tsan, which failed at link
  (`multiple definition of operator new` against `libclang_rt.tsan_cxx`), the cause c99e6be guards.

## What I checked myself

- Refusal A/B, job 261124-182925-f7af69 (exit 0; script
  `.scratch/xapiand/review-prospect-pool-reader-transient-correctness/refusal_ab.sh` and
  `refusal_ab.cc`, run from the quest's slot at c99e6be). One program built twice, against base
  prospect:44303965's `include/` and the fix's. It runs on dictionaries of 4 and 10 fields (one own
  dictionary, the rest pooled). It feeds every single-byte change of the terms and the index (masks
  0x01, 0x80 and 0xFF) and a verifier that rejects each terms offset in turn. Each case runs with
  and without a pooled query before `check_fields`, then runs a pooled query after it. It records
  each outcome's exception type and message, plus the verifier's call count. Result: 43,062 lines,
  identical. The outcomes it reached: ok, "terms checksum", "term index", "pooled field id", "field
  id outside the field table", "field directory" (sizes, sample, flags, entry), "truncated integer".
  So refusals are unchanged on these inputs, the verifier sees the same calls, and a query after a
  failed or passed check behaves as before.
- Q1, by reading. `pool()` and `open_at` are unchanged byte for byte. `dictionary()` now forwards
  to `transient(e.blocks, e.blocks_size, ...)`, the same body with `e.blocks` renamed `base`, so the
  cursor's own-dictionary path (`own_ctx_` a cursor member) is unchanged. In `check_fields`,
  `dict_context ctx` is declared before the reader (include/prospect/fielddict.hh:589-590), so it
  is destroyed after it. The reader is a prvalue (guaranteed elision) and dies at the end of the
  call; it views `terms_` and `pool_index_`, which the caller keeps alive past the call.
  termdict::reader (pin e3f6f548, termdict.hh:827-845) stores only the verifier and context
  pointers and calls them in `open()` during the scan, never in its destructor. Every other pool
  reader (find, scan, cursor, seek cursor, add_plan) still goes through `pool()`. `transient` has
  no other caller.
- Q3. The mutant job's log fails only at test/fielddict_test.cc:445 and :447, the two checks after
  `check_fields`, so the test is a value test that catches a return to caching. Under TSan,
  `HEAP_PROBE` is 0. The heap checks then compare an unchanging counter, so they pass vacuously,
  while the lookup, verifier-call, verifier-refusal and "pooled field id" checks still run. The
  CI failure it fixes is the link clash in run 45516118177 (linux-tsan, clang 18).
- Q4, by arithmetic on 261124-160830-97749f's log. Every number in the doc's table matches the
  log. The +1.57 us and +1.70 us figures and the A/A bands are right. The `verify_in`-per-block
  claim holds: termdict.hh `open()` verifies on every block open, and no reader caches it. See F1
  for what the profile does not cover.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | The doc names the cause as "the repeat is the FST validation". Its profile puts one pool reader build at about 0.94 us (fst::map about 0.96 us), but the measured first-query cost is +1.57 us (4 fields) and +1.70 us (10 fields). By the table, fix `check`+`query` minus `check` is 2.36 us against base's 0.32 us, so about 0.6 to 0.7 us per segment is unexplained. The follow-up's line "the first-query row would equal the base's" with a trusted constructor is a prediction, not a measurement. The trade's size is stated honestly; only its full attribution is not. | 261124-160830-97749f log; docs/perf/fielddict-pool-reader.md "Where the second build goes" | Say that the profile explains about 0.95 of the 1.6 us, and call the trusted-ctor saving an estimate. Or profile the build inside the harness's 1,000-segment loop. |
| F2 | no | nit | Under TSan the heap checks are compiled to vacuous comparisons rather than kept live. The review traps name `__sanitizer_malloc_hook` (or `__sanitizer_install_malloc_and_free_hooks`) for exactly this. The property is single-threaded and runs on every other CI platform. | test/fielddict_test.cc:18-40 | Optional: count blocks through the sanitizer hooks under TSan. |
| F3 | no | nit | The test checks the failing-verifier case by exception type only, not by message. The A/B probe shows the messages equal base's. | test/fielddict_test.cc:477-486; 261124-182925-f7af69 | Optional: assert "terms checksum" / "term index" as the probe sees them. |
| F4 | no | nit | The quest file reads field-check-at-map's d1000s4 and d1000s10 as "1,000 segments with 4 fields"; hunt.md:570 means 1,000 docs over 4 or 10 segments. The doc reads them correctly as segments (416 B x 4 = 1.7 KB). The lead's text is off; the artifact is right. | .campaign/xapiand/hunt.md:570 | None in the quest branch. |

## Not run

- CI at c99e6be (run 45531065901 was queued when I finished): A2's platforms, linux-tsan
  included, are the lead's to read.
- No rerun of A3: `include/**` and the harness are unchanged since its commit.
- No whole-index (strata eager_open) measurement; the per-index figures rest on 416 B x segments
  and on field-check-at-map's d1000s4/d1000s10 numbers in hunt.md.
