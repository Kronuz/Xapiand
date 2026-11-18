# hunt-radix-router review: correctness

Commit: radix-router:2aa9b22fa47b2d904492165d888ad7e0120a8819
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261117-194919-491cc8 (A2 test) and its fails-without runs against main: the k=6 n=64 test
  passes at the fix and fails on main's matcher.
- 261117-194920-fa5d3f (A3): `fuzz/diff.cc` identical to main over 200k random sets and the
  corpus and artifacts. I checked `fuzz/radix_router_main.h` is main's `radix_router.h` with only
  the namespace renamed (3 lines differ).
- 261117-194506-e318e7 (A4) and the k=8 n=64 207 s to 0.29 ms numbers, taken as given.

## What I checked myself

Scripts in `/Users/kronuz/code/Kronuz/Xapiand/.scratch/review-hunt-radix-router-correctness/`
(`run.sh diff|scale|threads`, `early-return.sh`), all at 2aa9b22, light.

- Memo key soundness by reading `radix_router.h` `match`/`dispatch`: every `path` passed down is
  `substr(offset)` of the lookup path, so remaining length names the suffix exactly; matching
  never reads `params`; a recorded state failed with only correct entries in the set, so by
  induction every entry is a true failure and pruning removes only branches with no hit. Priority
  (static > param > catch-all, shortest catch-all capture first) and captures are therefore those
  of main's DFS: pruned branches add nothing and had restored `params` on failure.
- 261117-195323-ba783c `run.sh diff`: `diff2.cc` against main's header, 4 seeds x 3M lookups,
  route sets of up to 10 routes with up to 4 nested catch-alls per pattern (8 when stacked on a
  shared prefix), params between catch-alls, mid-segment wildcards (`/ab*p`, `/x:u`), trailing
  slashes, routes differing only after a shared catch-all prefix; paths of up to 13 segments from
  route tokens with empty segments and trailing slashes. Compares value, every capture's name and
  text, and the capture's pointer into the path. Identical on all 12M; about 225k hits per seed
  on routes with 2+ catch-alls.
- 261117-195335-84ae6e `run.sh threads`: 8 threads on one shared router under TSan, 156 paths x 20
  each, results equal to a single-threaded pass, no TSan report. The `Memo` is a local in
  `find()`; `Node::match` is const and touches nothing else.
- 261117-195328-8e9077 `run.sh scale`: failing lookup time for k nested catch-alls over n `/x`.
  Time grows 4x per doubling of n for every k: k=2 n=4096 (8 KB path) 160 ms, k=4 513 ms, k=8
  1.21 s. See F1.
- 261117-195626-9b2ab7 `early-return.sh`: the F1 fix applied to a copy of the header. Time grows
  2x per doubling (k=8 n=4096: 6.0 ms); `diff2` identical to main on 2 seeds x 3M lookups.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | Matching is still quadratic in path length, not linear as the quest's Outcome, A2 and the note's "O(nodes x path length)" say. A nested catch-all is re-entered once per parent state and its loop walks every later position each time, doing a set lookup per skipped state: O(k x n^2). At an 8 KB path, k=8 takes 1.21 s and even k=2 (quadratic on main too) 160 ms. | 261117-195328-8e9077; `radix_router.h` CatchAll case, the `memo.failed.find(state) == memo.failed.end()` branch | Treat a hit in `failed` as the end of the loop: `if (memo.failed.count(state)) { return nullptr; }`. Sound because the loop from a position depends only on the suffix there and an earlier entry of this node, already finished (a node is never its own descendant), tried every later position and failed. Probe 261117-195626-9b2ab7: linear (k=8 n=4096 6.0 ms), identical to main on 6M lookups. Test: k=8 over 4096 `/x` fails under a bound such as 100 ms, failing without the fix at 2aa9b22; fix the Memo comment and the note's bound and numbers. |
| F2 | no | gap | A2 asks for work at most a stated multiple of path length; the test asserts wall time at k=6 n=64, which cannot tell O(n) from O(n^2) (both are well under 0.5 s there). | `test/test.cc` `test_nested_catch_all_bounded` | Covered by F1's test at n=4096; a step counter would make it exact but is optional. |
| F3 | no | nit | `fuzz/diff.cc` paths have at most 6 segments over `a`/`b` only and no empty segments, so nested catch-all memo hits are shallow there. | `fuzz/diff.cc` generator | Optionally adopt `diff2.cc`'s generator (deeper paths, route tokens, empty segments, mid-segment wildcards). |

## Not run

- No strict timing; the scale numbers are light-hold wall times, used only for their growth ratio.
- No sanitizer other than TSan, and only on macOS clang; ASan/UBSan and Linux are CI's.
- Did not rerun A2, A3 or A4, nor check the A5 note beyond its bound claim.

## Re-check

Commit: radix-router:234c236
Verdict: APPROVE
Date: 2026-11-17

Scope: `git log 2aa9b22..234c236`. F1 is fixed: in `match_memo`, a memoized failed state now ends the catch-all's loop. Lookups use the memoized matcher only when `nested_` is set, i.e. after a pattern with 2+ `*` was inserted. Otherwise `find()` runs the plain `match`/`dispatch`, which differ from main's only by one blank line (awk extract and diff, run by hand).

1. **The flag is exact.** `add_route` reaches a node below a catch-all only by consuming that catch-all's sigil from the same pattern. That is the `n = n->catchall.get()` step, taken only for a `*` in the pattern being inserted. An edge split in `descend_static` moves `catchall` to the new child, which keeps ancestry. So patterns with one `*` each that share nodes can never put a catch-all below another catch-all. A `*` cannot appear in static text or in a wildcard name, because `descend_static` stops at it and `add_route` throws on it. So counting `*` per pattern over-approximates nesting by nothing. A flag left set after a throwing insert only selects the memoized matcher, which agrees with the plain one.
2. **Both matchers agree, including when the flag flips mid-way.** 261117-224037-60671d: `diff2.cc`, with lookups interleaved after every insert, compared against main's header over 4 seeds x 7.1M lookups. It found zero differences in value, capture names, capture text or pointers. Of those lookups, 6.2M ran the memoized matcher and 0.93M the plain one.
3. **The linear bound holds in the memoized matcher.** 261117-224042-2bff5b `run.sh scale`: time grows about 2x per doubling of n for k=2, 4 and 8. At k=8 n=4096 (8 KB path) it takes 5.1 ms, against 1.21 s at 2aa9b22. The suite's new k=8 n=4096 < 0.1 s check (bound test e86d08, fails-without a4772a) now carries F1's test, which also closes F2.

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | nit | The Memo comment still says "lookups with a single catch-all never touch the set". This is true, but such lookups now never reach `match_memo` at all. | `radix_router.h` Memo comment | Optional rewording. |

Not run in the re-check: strict timing, other sanitizers and platforms (CI's), and the A4 exclusive A/B (taken from the lead's job).
