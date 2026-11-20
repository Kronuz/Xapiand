# dsl-not-wrap review: correctness

Commit: Xapiand:4977e85aa483511f1917572d3dc2903d66e7e5d8
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261119-154841-203822: both tests pass at 3959a8c85 (macOS, Apple clang 17). src and tests are
  byte-identical from 3959a8c85 to 4977e85aa (`git diff --stat` is empty for query_dsl.*,
  test_query_dsl_not.cc and test_coverage_core_query_dsl.cc).
- Suite 261119-155154-32bbee passed.
- Mutant branch quest/xapiand/dsl-not-wrap-mutant, which carries the pre-fix query_dsl.cc.
  1. arm64 CI 44643000441 (g++-13) fails test_query_dsl_not.cc:51, :52 and :62.
  2. x86 CI 44642999982 (g++-13, TSan and ASan+UBSan rows) fails the same lines plus
     test_coverage_core_query_dsl.cc:446. Its log prints `{"_not":[[{"a":{"_value":"b"}}]]}`.
  3. FreeBSD CI 44643000126 (clang) and macOS job 261119-154845-7b6ce7 pass.
  So the A1 test fails without the fix on both GCC rows.
- Fixed code on GCC: arm64 44660086464 and FreeBSD 44660086506 are green at 4977e85aa. All three
  workflows were green at b3d36bcd5, which has the same code (CI 44641195956, arm64 44641195951,
  FreeBSD 44641195962). x86 CI 44660086422 at 4977e85aa was still running when I wrote this.

## What I checked myself

Probe job 261119-182703-6bff71 ran at 4977e85aa on macOS from
`.scratch/xapiand/review-dsl-not-wrap-correctness/run.sh` and `probe_body.cc`. It appends probes
to the coverage test's fixture, builds them and runs them. Its output is saved beside the scripts
as `probe-261119-182703-6bff71.txt`.

The job exited 1 on two checks. Both failures came from my own wrong expectation about
`k:a AND NOT k:b AND NOT k:c`, which the code builds as two nested `_and` clauses (see N3). I
corrected that expectation in the script after the run and did not rerun it. Its other 46 checks
passed.

1. **Q1, the mechanism.** In `object[op] = {{ x }}`, overload resolution picks
   `operator=(std::initializer_list<MsgPack>)` on every compiler, because an initializer_list
   conversion ranks best. The compilers split on the next step, when the one list element is
   initialized from `{ x }` and `x` is itself a MsgPack. A compiler that applies CWG 1467's rule
   copies `x`, which gives the flat `[x]`. A compiler that follows CWG 2137, which limits that rule
   to aggregates, goes to MsgPack's initializer_list constructor (msgpack.h:135, Kronuz/msgpack
   1e9aaaf) and gets the nested `[[x]]`. CI matches this: g++-13 on both arches nests, and Apple
   clang 17 and FreeBSD clang stay flat. The construct has no undefined behavior; the split is
   divergent initialization rules. I read Clang's CWG 2137 status (adopted in Clang 20) from
   memory and did not verify it, so a FreeBSD or Xcode upgrade could have started nesting there
   too. The fix uses `ARRAY()` plus `push_back` (query_dsl.cc:1324-1327), which needs no
   initialization rule.
2. **Q1, the grep.** `grep -n '{ *{' src/query_dsl.cc` at origin/master finds only :394 and :1321.
   Both are rewritten, and the grep is empty at 4977e85aa. The :394 cast `{{ name, o }}` was not
   divergent: its inner list has two elements, so it is always the pair that `_initializer` turns
   into `{name: o}`. The rewrite to `cast[name] = o` builds the same map. In the rest of src
   (`--exclude-dir=xapian`, sortable_serialise excluded), no `{{ expr }}` of a single MsgPack is
   left. The one single-brace `MsgPack({ exc.what() })` at search_views.cc:2708 wraps a
   `const char*`, so it is not affected. `create_2exp_op_dsl`'s `{ nullptr, x }`
   (query_dsl.cc:1339) has two elements, and its first is not a list, so it always builds a list.
3. **Q2, the trees.** The probe pins the exact tree, the exact engine query, and the property "no
   list directly holds a list" for these inputs:
   - `_not` nested once and twice.
   - `_not` over `_or`, `_and` and `_and_maybe` groups of 2 and 3 operands.
   - `_not` under `_and` and under `_and_maybe`.
   - MAYBE chains of 2 and 3 operands, and left-parenthesized MAYBE.
   - `NOT (NOT a OR NOT b)`.
   - `_and_not` with 2 and 3 operands, and with a nested `_not`.
   - DSL objects with `_not` over 1 and 3 operands.

   All of them pass. No query string produces `_and_not`, so it is DSL-object input only and the
   fix does not reach it. Every `_not` tree now comes from `create_exp_op_dsl`, which no longer
   depends on the toolchain. The other builders use `push_back` or two-element lists, which are the
   same everywhere. GCC CI runs the quest's own tests green.
4. **Q3, what output changes.** Nothing changes on clang builds (macOS, FreeBSD). On GCC builds,
   which include the Linux CI and the release Docker and Linux bottles, three things change:
   - **The search and count response at verbosity 3 or higher with comments**
     (search_views.cc:2026-2027 and :2162-2163). `#query` changes from `{"_not":[[x]]}` to
     `{"_not":[x]}`. `#engine_query` loses an OR of one: the probe shows the old GCC tree as
     `(<all> AND_NOT ((k/a)))` and the new one as `(<all> AND_NOT (k/a))`.
   - **The `qdsl:` and `query:` debug logs** (handler.cc:1277, :1280) change the same way.
   - **The depth limit for chained NOTs.** The old GCC tree used three `process()` frames per NOT
     instead of two. The probe accepts 44 chained NOTs with the fix and 29 with the old GCC tree,
     so on GCC, chains of 30 to 44 NOTs are now accepted instead of refused.

   Matching does not change, because an OR of one matches what its child matches. The
   `make_dsl_query` tree is not stored and is not sent between nodes: its only consumers are
   `get_query`, the log and `#query`. I found no stored or wire format that changes. No test or doc
   pins `#query`, so nothing has to be updated.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | Pre-existing and outside this quest. A right-parenthesized MAYBE reorders its operands: `k:a MAYBE (k:b MAYBE k:c)` builds `(k/b AND_MAYBE k/c AND_MAYBE k/a)`, which makes `a` optional and `b` required. The same merge in AND, OR and XOR is harmless because those operators are commutative. | probe 261119-182703-6bff71 "print" line; query_dsl.cc:1174-1180 (the right operand's list takes the left with `push_back`) | Hunt row: when the right operand is the merged one, insert the left operand at the front, or keep `(a MAYBE (b MAYBE c))` nested. |
| N2 | no | nit | The new comment at test_coverage_core_query_dsl.cc:445 is correct. The quest file's Outcome, though, says "nested on macOS/Linux, flat on FreeBSD". The split is GCC against clang: macOS was always flat. | mutant CI 44642999982, 44643000441, 44643000126; job 261119-154845-7b6ce7 | Lead: correct the Outcome text or the hunt row if it is kept. |
| N3 | no | nit | Pre-existing. A NOT inside an AND run stops the run merging: `a AND NOT b AND NOT c` builds `{"_and":[{"_and":[a,{"_not":[b]}]},{"_not":[c]}]}`. It matches the same documents on every toolchain. | probe 261119-182703-6bff71, the two failed checks | None needed. |
| N4 | no | nit | On GCC builds the fix changes the `#query` and `#engine_query` comment fields at verbosity 3 or higher, and raises the accepted NOT-chain depth from 29 to 44. It also makes these the same as the clang builds. This is a correction; worth a line in the landing commit. | Q3 above | Mention it in the commit or changelog. |

## Not run

- I did not compile or run anything on GCC myself. The GCC behavior, before and after the fix,
  comes from the CI runs above. x86 CI 44660086422 at 4977e85aa was still in progress; A3 is the
  lead's to close on it.
- I did not rerun the probe after correcting the N3 expectation.
- I did not run a search end to end through HTTP to see `#query`. The Q3 answer comes from reading
  the code and from the probe's engine-query output.
- I did not verify Clang's CWG 2137 version.
