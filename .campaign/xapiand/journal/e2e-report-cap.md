# e2e-report-cap: journal

State: waiting on jobs 261111-013408-376057
Quest: .campaign/xapiand/quests/e2e-report-cap.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Exploration. Cause (from job 261110-152519-7a2db9 log + newman 6.2.3 source): the json reporter does JSON.stringify(summary, 0, 2); each response body is a Buffer serialised as {type:Buffer,data:[one number per line]} (~19 bytes per body byte; 3.3M lines = 62.9 MB). e2e_assert.py reads status codes, assertions and the body only of failing requests. Exp 1 (job 261111-013408-376057, ~/.scratch/e2e-report-cap/exp1.sh): reproduce the full report and see why it does not parse.

## Next

1. Read exp1 output; pick the fix (slim newman reporter written by e2e_capture.sh vs filter); run the e2e; break an expectation for A2.

## Blocked on

Nothing.

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
