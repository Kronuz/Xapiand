# xapiand-final-release: journal

State: waiting on jobs 261120-202653-9ebd4f 261120-202700-a1e84b 261120-202710-3443a6 261120-202717-edd5aa
Quest: .campaign/xapiand/quests/xapiand-final-release.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Backports on the quest branch (based on the `xapiand` tip e692a64b1, so the tool's worktree base fits). Done: parser cherry-picks (4b9ee717e, 8ab6b2b26) with the adapter on by default and a nested NEAR test; id 0 (971f90559, d0c9a5733) with the harness check added to the smoke layer; A3 (fdb32df96); changelog (72ebbfa83). Job 261120-155714-a3ba72 failed (getStopWords include resolved to the wrong stopper.h); fixed; build-test 261120-172811-fc88f8 failed 3 of 61 checks of test_generate_terms (lines 334-336, the positive/floating terms-only loop); base-commit run (e692a64b1) is job 261120-174741-3e4b95 to tell pre-existing from mine.

CI on `xapiand` at bb04f6d63 failed twice (lead's log ~/.scratch/xapiand/lead/xapiand-branch-ci.log):
1. test_text_parser.cc's `#include "xapian.h"` resolved only through Homebrew's /usr/local/include (Xapian 2.1.0) locally; Linux has none. Fix 5c4c0ce2d: src/engine/xapian (the vendored umbrella) first on that target's include path. Guard job 261120-202710-3443a6 (dep file names the vendored header, never /usr/local/include); 261120-202717-edd5aa is the same check failing at bb04f6d63.
2. smoke-id0 under ASan/TSan (222 and 452 of 999 kept). Cause: Schema::index tested `!document_id`, and MsgPack's bool of 0 is false, so a body's `"_id": 0` counted as no id (the URL-id refusal in get_prefixed_term_id never ran on RESTORE). On a new index doc 0 then set the id type to uuid while the others prepared beside it; when the numeric type won instead, id 0 became the new-id request and id 1 overwrote it, which is why the plain macOS run passed (job 261120-202217-a29843). Fix 91d4c3e62: any number is an explicit id; numeric id 0 throws ClientError. The harness now also requires RESTORE's item 0 to be 400 and `indexed` 999, and ctest runs it (5c4c0ce2d), so the local build-test catches it. Fails-without: 261120-202653-9ebd4f at 5c4c0ce2d; build-test at the fix: 261120-202700-a1e84b.

## Next

1. Read the four jobs above; on green, report to the lead (the lead pushes `xapiand`). Then: push the quest branch, then fast-forward `xapiand` to it and push (CI there: build-test, asan, e2e, Release/FreeBSD/Bottles dispatch).
2. A5: read CI and the three workflows at the final commit; report to the lead. No tag.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Tool flow: the quest branch already sits on `xapiand` (worktree base fits); `campaign land` lands on main, so landing is `--record` after the tag, as the quest says. `campaign brief` flags "master changed 1485 files" and out-of-file-set changes: expected, this branch is not master's.
- A1: the adapter was opt-in (XAPIAND_QUERYPARSER=1); made default, =0 selects Xapian. test_text_parser pins =0 as its reference.
- A2: the id-0 check was a harness script only; run_all.sh smoke runs it, and ctest too (5c4c0ce2d): the local build-test is ctest only and never ran smoke.
- A2 fix lives in src/database/schema.cc, outside the listed file set: the lead assigned the restore-path fix. Explicit `_id` of false, null or "" still asks for a new id (unchanged); only numbers became explicit, so `"_id": 0` on a uuid-id index is now a cast error instead of a random uuid.
- A3 float bounds: lower bound ceil, upper bound floor, saturating, in range.cc integer_bound.
- test_generate_terms failure (job 261120-172811-fc88f8) was mine: a non-negative int can carry the NEGATIVE_INTEGER tag, so integer_bound emptied positive ranges; passes at base (job 261120-174741-3e4b95). Fixed by reading the sign from the value, regression checks added; build-test 261120-181401-b5cc95.
