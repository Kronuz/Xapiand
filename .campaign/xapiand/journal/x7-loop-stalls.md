# x7-loop-stalls: journal

State: waiting on jobs 261111-201659-919f00
Quest: .campaign/xapiand/quests/x7-loop-stalls.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 (profile note, `docs/perf/x7-loop-stalls.md`; file set is only that note). Binaries: x7-gate's build job
261111-171014-19268b, copied to `$HOME/.scratch/x7-loop-stalls/49ebb4481/xapiand` (master's src; build-cur) and
`.../142b2ca77/xapiand` (pre-7c). Jobs run at x7-gate head Xapiand:234fe089a for its harness.

- RESTORE: from x7-gate's t12b samples (261111-171048-c39d8d, `bulk/pb-*.sample.txt`), no new job: one reactor thread is
  busy for the whole restore (5,858 of 6,816 samples); pre-7c spread it over 10 DP and ~5 DI threads.
- Loops: E1 261111-201659-919f00 (exclusive): `sample` of the T4/T5 mix, then 12 s loaded phases per heavy kind,
  restore alone, and the mix on 8 reactors. Analyzer: `.scratch/st.py` (untracked).

## Next

1. Read E1's output; write the loop section of the note; rank fixes; commit; report to the lead.

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
