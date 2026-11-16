# strict-values review: correctness

Commit: Xapiand:710beb9f3f9ac372a02ce23b2a15f5f115534ee5
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261116-092510-5a755e (done, exit 0): the five D92 node cases pass at 710beb9f3. The tests check
  hits by value, statuses and message text, not only counts.
- 261116-092513-8e8db4 (done, exit 0) was **not** accepted as evidence. Its log shows the master-src
  build failed (`use of undeclared identifier 'restore_line'`). The cut in
  `.scratch/strict-values/cases_on_master_src.sh` removes everything from "// Serialize::exact, the
  one rule:" to "// The schema of the indexes below:", and `restore_line` is defined in that span.
  The script also exits 0 on a build failure and exits 1 when every case fails on master. That is
  the reverse of `--fails-without`, which counts a job that exits 0 (campaign.py `tagged_row`). The
  three queued siblings (7d49d2, a7c977, 55e91e) use the same script.
- build-test 997086 and e2e 845129 at 710beb9f3 were still queued when I wrote this.

## What I checked myself

I read the code in src/serialize.cc, src/cast.cc, src/query_dsl.cc, src/multivalue/range.cc,
src/database/schema.cc and src/aggregations/bucket.{h,cc}. I also read strict_stox.hh, which says
the declined path follows `strtoull`, including its leading whitespace and its negation of "-N".

I submitted three probe jobs. All three were still queued when I handed back, so their results are
not in this review. Their scripts are under `.scratch/review-strict-values-correctness/`:
- 261116-094624-2526c7 `probe.sh`: probes at head for values above 2^53, 1 and 1.0 on a floating
  field, untyped, dynamic and non-numeric fields, a multi-index search with conflicting types,
  arrays, a body `_id` against a path `_id`, and out-of-range casts on writes and searches.
- 261116-094633-c057e3 `probe.sh 26ab9c5a1`: the same probes on master's src, plus the four D92
  cases with a correct cut. It prints each case's verdict and failed checks. This is the honest
  fails-without check.
- 261116-094810-63fe0d `probe_wrap.sh`: whitespace and minus wraps, `0x10`, and `"…993.5"`.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | yes | gap | The four `--fails-without` jobs prove nothing. The cut deletes `restore_line`, so master's build fails and the job exits 0, which campaign counts as "fails without the fix". The exit codes are also the reverse of what campaign expects. A1 to A4 have no valid failing-test evidence. | job 261116-092513-8e8db4 log; `.scratch/strict-values/cases_on_master_src.sh` | Cut only the unit case (to "// A document written by a RESTORE line"). Exit 0 only when the build succeeds and every case fails, and print the failed checks so the failure reason shows. 261116-094633-c057e3 does both. |
| C2 | yes | defect | R1 is not exact above 2^53. An integral double of that size need not be the integer the client sent. `{"i": 9007199254740993.5}` and `"9007199254740993.5"` round to the double 9007199254740994, `trunc(d)==d` holds, and an integer field stores and matches 9007199254740994. A fraction is accepted silently, which goes against the quest outcome and the docs ("nothing is truncated"). `"9007199254740993.0"` gives 9007199254740992. The same happens on `_id`, in searches and in range bounds. | src/serialize.cc `exact_number`, FLOAT case (`std::trunc(d) != d`); probe 261116-094624-2526c7 (queued) | For integer and positive types, refuse a float with \|d\| >= 2^53, since no such double proves its integer. Text spelling an integer still goes through stoll/stoull and stays exact. Add a test with 9007199254740993.5 and "9007199254740993.0" (400 on write and search). |
| C3 | yes | defect | Text with leading whitespace wraps. `number_of` guards `text.front() == '-'`, but `strtoull` skips the whitespace first. So `" -18446744073709551615"` reads as uint64 1 and `" -9223372036854775809"` as 9223372036854775807, and an integer field takes both. `Cast::positive` has the same guard, so `{"_positive": " -5"}` reads as 2^64-5. This contradicts "a cast never wraps". | src/serialize.cc `number_of`; src/cast.cc `Cast::positive` STR case; probe 261116-094810-63fe0d (queued) | Reject leading whitespace (or check the first non-space byte) before `strict_stoull`, in both places. Add cases to `test_exact_reads_a_value_as_its_type` and to the cast test. |
| C4 | no | gap | Removing the fallback narrows multi-index searches. The schema comes from `endpoints[0]` (schemas_lru.cc:322). If a field is integer in the first index and text in another, `/a,b/?q=m:word` and `/_all/` with the same query now answer 400; master answered 200 through the global query. That is the owner's call. Options: (1) keep the 400; (2) fall back globally only when there are several endpoints; (3) type each index's query by its own schema. | src/database/schemas_lru.cc:322, src/query_dsl.cc `get_value_query`; probe 261116-094624-2526c7 (queued) | Owner decides; document whichever behavior is chosen. |
| C5 | no | nit | Text reads more loosely than JSON: `"0x10"` is 16 (hex float through strtod), and `" 12"` and `"+12"` are accepted. That contradicts R1 ("as JSON reads a number"). | src/serialize.cc `number_of` | Accept only JSON number grammar. |
| C6 | no | nit | The `_term` filter aggregation still reads values with `Serialize::MsgPack`, not `exact`. It refuses `12.0` on an integer field, which a search accepts, and its message does not name the field. It is a second reader outside the quest's list. | src/aggregations/bucket.cc:70,74 | Route it through `Serialize::exact` in a follow-up. |
| C7 | no | nit | Range aggregation bounds take any number. This is exact by construction (ceil over integers), so it is not a bypass. Sort `_value` casts, per R5. | src/aggregations/bucket.h `reach_of`; src/query_dsl.cc:1475 | None. |

## Not run

None of my three probe jobs had run when I handed back, so C2 and C3 rest on reading the code
(`strtoull`/`strtod` semantics), and C4 on reading the code plus the journal's `/_all/` E2E failure.
Without the probes, I have not confirmed by run:
- That a floating field matches 1 and 1.0 on search, though the encoding is the same.
- Untyped and dynamic field searches, and `q=` ranges on an untyped field. By the code they match
  master.
- Arrays and a body `_id` against a path `_id`.
- Out-of-range casts on date fields.

I did not check Lua-set fields beyond reading the code: they go through `Schema::index` and
`index_item`. I also did not check the build-test or e2e gates, which are still queued.
handler.cc:1639 was out of scope (already filed).
