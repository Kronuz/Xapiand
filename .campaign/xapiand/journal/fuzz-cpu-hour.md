# fuzz-cpu-hour: journal

State: working
Quest: .campaign/xapiand/quests/fuzz-cpu-hour.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 measured (ASan replay profile + libFuzzer exec/s at prospect:61e09a6); harness cuts in the
worktree (fielddict base dictionaries cached, postings seeded-top-k size by input bits, replay.cc
REPLAY_COST); `tools/release_rows/fuzz_hour.sh` (build, cost, run: CPU-time budget, persistent corpus
~/.scratch/fuzz-corpus/T, -max_len with -len_control=0 for segment). A/B of segment base sizes and
length limits running as scratch probes; then docs/perf/fuzz-targets.md, then submit the jobs.

## Next

1. Decide segment base size and -max_len from the A/B runs (scratch, .scratch/ab).
2. Write docs/perf/fuzz-targets.md (A1), commit, push the branch.
3. Submit: build jobs (before = base commit, after = head), cost jobs, four 3600 CPU-s runs (--for A2,A3).

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
