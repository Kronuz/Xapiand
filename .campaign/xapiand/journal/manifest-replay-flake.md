# manifest-replay-flake: journal

State: waiting on jobs 261111-093034-13afb8 261111-093035-315dd2 261111-093037-f580c7 261111-093039-d608a0
Quest: .campaign/xapiand/quests/manifest-replay-flake.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

replay_cost now takes the median of nine paired (small, then big) ratios (strata:51bbe66a, pushed). Jobs: suite (A1), mutants identity-scan and name-scan (A2, local branches mutant/replay-mut-identity, mutant/replay-mut-fields), 20 runs under a parallel ctest (A3). Counting work was not possible: apply() exposes no counter and non-test files are out of the file set.

## Next

1. Read results; if A3 fails, raise the pair count or the small run's edit count.
2. Check CI green on the pushed branch; fill Evidence and Result; State ready for review.

## Blocked on

<What, and who decides. "Nothing" when nothing.>
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

Timing, not counting: no natural counter in manifest_log.hh and it is outside the file set. Bounds unchanged (40 and 20, vs ~100x for a scan); pairing plus median replaces fastest-of-five.
