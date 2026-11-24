# hunt-manifest-log-wallclock: journal

State: waiting on jobs 261123-174439-9fb117 (A1 build-test at strata:277e595d), 261123-174446-9bce6e (A2 mutant)
Quest: .campaign/xapiand/quests/hunt-manifest-log-wallclock.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A3 (form chosen): ratio to a same-process calibration. In `deletes` (test/manifest_log_test.cc), the
128-edit apply and the log's open read are each the fastest of five runs divided by the fastest of
five reads of the same snapshot alone. Counting work does not fit: the library has no counter
(test-only quest), the harness's operator-new byte count is off under TSan and cannot see a walk
(clumped and sparse allocate nothing), and the at_bound check beside it already uses ratios.

Healthy ratios (probe 261123-173046-c009d9, Release, strata:761da2c7): apply 0.001 / 0.009 / 0.031
(10M deleted / clumped dense / sparse below), log read 0.99 / 0.97 / 1.14 (snapshot unit 22 / 2.5 /
2.1 ms). Bounds set at strata:277e595d: apply < 2, read < 4 (60x over healthy apply; the 8.9 s
regression is about 400x the unit). time_scale and its sanitizer macro removed.

A2 mutant: 7b246c1f (not on the branch, parent 277e595d) adds `auto rewrite = ordinals()` after the
dense mark in log_deleted::insert, the per-edit ordinal-vector rewrite.

## Next

1. A1 and A2 jobs finish; paste `campaign brief`'s table; push; CI (A4).

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
