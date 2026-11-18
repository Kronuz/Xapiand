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
