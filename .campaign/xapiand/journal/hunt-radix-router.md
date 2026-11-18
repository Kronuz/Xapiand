# hunt-radix-router: journal

State: working
Quest: .campaign/xapiand/quests/hunt-radix-router.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

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

<Decisions inside the quest's scope, so the reviewer can check them.>

## State (contributor)
Waiting on jobs 261117-184755-fff446 (strict; A1 timing of the retained slow units and the nested catch-all family k x n at main's matcher).
Hypothesis 1 (experiment 1 of 3): non-final catch-all backtracking (radix_router.h match(), CatchAll case) is exponential in nested catch-alls: O(n^k).
Committed radix-router:c2542f5 (fuzz/slow_repro.cc, fuzz/regressions/). Next: read job output; if it confirms, fix by memoizing failed (node, suffix-offset) states, then A2-A5.

## State (contributor, update)
Fix committed in radix-router (memo of failed catch-all states, Memo in find()); docs/NESTED_CATCHALLS.md (A5).
Waiting on jobs: A2 261117-193212-3690a3, A2 fails-without 261117-193223-6651ac, A3 261117-193233-900a86, A4 strict 261117-193249-dddd3f.
A1 evidence: 261117-184755-fff446. Exposure: Xapiand uses fixed internal tokens, no catch-alls; not reachable by client path.
Rulings I made: memoize only catch-all continuations (static/param branching is depth-bounded); unordered_set allocated lazily.
Next: record numbers in A4 and the note when jobs finish.

## Fix round 1
A4 regression cause (counted, not timed): failing single-catch-all lookups inserted into the heap set (5 of 14 paths in the mix). Gated memo by catch-all nesting; 0 inserts on the mix now.
Waiting on jobs at the new commit: A2 261117-193713-8b0cc1, fails-without 261117-193724-43d383, A3 261117-193733-999a72, A4 strict 261117-193741-7a68ec.

## State: waiting on jobs (then ready for review)
Evidence: A1 261117-184755-fff446 (main: k=8 n=64 207 s) and fixed-time rerun 261117-194859-accfbe; A2 + fails-without + A3 rerun at 2aa9b22 (ids from `campaign job rerun`); A4 261117-194506-e318e7 (fix/main 1.010 [0.944, 1.080] vs A/A 1.097 [1.006, 1.196], 10 rounds); A5 radix-router docs/NESTED_CATCHALLS.md [artifact].
Not pushed (contributor rule); lead pushes. Fixed k=8 n=64 time is in job accfbe's output.

## Review F1 folded (radix-router:69d3dbf)
F1: loop now returns on a memoized repeat; test k=8 over 4096 `/x` < 100 ms; Memo comment and note corrected. F2 covered by that test. F3 not adopted (optional).
Waiting on jobs at 69d3dbf: A2 261117-200150-9ef2b1, fails-without (vs 2aa9b22 header) 261117-200155-50edce, A3 261117-200200-153961, A4 strict 261117-200205-8b4389.

## Fix round 2 (radix-router:a38040a)
Suspect from reading, to be measured: Memo held an unordered_set by value (constructed+destroyed per find). Now a lazy unique_ptr. Diagnostic three-way strict job (main / 69d3dbf / current / A/A) 261117-203844-3a7578 tells whether it was that or the extra argument.
Jobs at a38040a: A2 261117-203849-6f4be8, fails-without 261117-203854-3d433a, A3 261117-203902-44f8af, A4 exclusive 261117-203906-3e8d00.
If not in the A/A band: kept old loop for non-nested is already the structure; next lever is dropping the memo argument via a separate nested-path function (not done).

## Re-plan: two matchers (radix-router:bd8172c)
insert() sets nested_ when a pattern has 2+ '*'; find() runs main's plain matcher (unchanged) unless set, else out-of-line find_memo. Test for flag + agreement on a mixed set. Note updated; its A4 and k=8 n=4096 numbers come from the jobs below (fill when done).
Jobs at bd8172c: A2 261117-212239-e86d08, fails-without 261117-212234-a4772a (2aa9b22 header + stub accessor), A3 261117-212244-bb5fb1, A4 exclusive 261117-212248-c7a142, timing (k=8 n=64/4096) 261117-212252-612504.
Rulings I made: flag counts any '*' pairs (conservative: includes final catch-all); set even if insert throws.

## State: ready for review (F1 confirmation) at radix-router:234c236
| Criterion | Evidence |
|---|---|
| A1 | 261117-184755-fff446 (main at c2542f5: k=8 n=64 207 s, k=6 n=64 4.18 s; artifacts 0.47-5.34 s); reproducer fuzz/slow_repro.cc, fuzz/regressions/ |
| A2 | 261117-212239-e86d08 (bound test, k=6 n=64 and k=8 n=4096 < 100 ms); fails without: 261117-212234-a4772a |
| A3 | 261117-212244-bb5fb1 (differential vs main, both flag states, corpus + artifacts) |
| A4 | 261117-212248-c7a142 exclusive 8 rounds: fix/main 0.920 [0.892, 0.949], A/A 0.996 |
| A5 | `docs/NESTED_CATCHALLS.md` |
Why 8% faster: plain matcher source is main's; disassembly sizes (nm, Apple clang -O2) show dispatch 880 B with match inlined vs 224 B in main's build. Code generation, not logic; not claimed as a win.
F1 confirmation: A2 test covers k=8 n=4096; the earlier reviewer probe measured 6.0 ms at the loop-return fix.
