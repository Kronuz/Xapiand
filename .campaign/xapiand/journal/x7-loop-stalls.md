# x7-loop-stalls: journal

State: waiting on jobs 261111-221539-18ce81
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

1. Read E1a's output (261111-221539-18ce81: `A-mixed.sample.txt`, `A-threads-*.txt`, `A-mixed.json`); fill the
   note's loop, fixes and reproduce sections. Only if the mix leaves the cause open: per-kind loaded phases
   (`.scratch/e1.sh`'s B runs, bare binary), each its own exclusive job well under 10 minutes.

E1 261111-201659-919f00 is invalid (exit 124 at its deadline): the head node exited 109 at start (CLI11's extras
error) because my wrapper passed `--solo`, which head no longer takes (x7-gate ran head's binary bare), and the
sampler's wait had no bound. E1a runs the bare binary and bounds the wait.

RESTORE section drafted (Xapiand:f1e29b97e): the accepting reactor is busy for the whole restore (83-86% of the
profile window, all three head arms); `finalise_write`'s decompress-and-recompress is 34-44% of that loop and
`Schema::index` 23-29%. Pre-7c ran the same work on 10 DP and 5 DI threads. Candidate fixes to rank: prepare off the
loop (in chores, or across loops) with only the apply in the turn; compress the record once (at finalise); apply per
shard in parallel (5 shards by default). Loop candidates to check against E1: unsliced work after the search (the
hit loop's `get_data` and JSON for `_limit: 100`, `search_views.cc` near line 2027), 4,096-ordinal windows with an
aggregation spy, per-segment scorer `build`, RESTORE's 0.5 ms per object between checkpoints.

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

- msg (lead, 11-11): E1 split; the mix sample first. RESTORE ruling recorded in the quest's Decisions (prepare as an
  index chore on the one WorkPool; compress once at finalise; parallel apply only if T12 stays short).

<Decisions inside the quest's scope, so the reviewer can check them.>
