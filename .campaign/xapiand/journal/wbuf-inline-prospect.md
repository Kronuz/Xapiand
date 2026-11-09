# wbuf-inline-prospect: journal

State: waiting on jobs 261109-105845-6fd04e 261109-105846-df7232
Quest: .campaign/xapiand/quests/wbuf-inline-prospect.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pin commit prospect:63e11d3 (strata 9d52824 -> 3df7af4ff4ac, pin line only) on the branch. Jobs: 6fd04e = strata test/prospect_integration.sh (prospect branch vs strata 9d52824; ctest + churn/dump identity at threads 1,2); df7232 = build-test check.

## Next

1. When both pass, fill Evidence (A1,A3 from 6fd04e; A2 from df7232) and Result; set ready for review.

## Blocked on

Nothing

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <job id at the current code> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
