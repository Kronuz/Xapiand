# hunt-manifest-log-wallclock: journal

State: waiting on jobs 261123-172410-fa7b2b (probe: healthy ratios), queued behind the host
Quest: .campaign/xapiand/quests/hunt-manifest-log-wallclock.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Form chosen: ratio to a same-process calibration (A3). In `deletes` (test/manifest_log_test.cc), each
of the 128-edit apply and the log's open read is the fastest of five runs, divided by the fastest of
five reads of the same segment's snapshot alone. Commit strata:7748cfa9 has the ratios printed and
bounds provisional (1e9). Probe job 261123-172410-fa7b2b prints the healthy ratios.

Why not counting work: the library exposes no counter (test-only quest), the harness's operator-new
byte count is off under TSan and cannot see a walk (clumped / sparse cases allocate nothing), and the
at_bound check beside it already uses this ratio form.

## Next

1. Read the probe's ratios; set bounds ~10x over healthy and far under the mutant.
2. Mutant (A2): a scratch commit making log_deleted::insert rewrite ordinals per edit (restore the
   8.9 s cost); job `--fails-without --for A2` on the test.
3. build-test `--for A1`; push; drop time_scale if unused.

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
