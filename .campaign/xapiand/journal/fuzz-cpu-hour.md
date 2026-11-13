# fuzz-cpu-hour: journal

State: waiting on jobs 261113-081412-4637f9 261113-081415-b2fb3a (cost before/after, strict, queued behind an exclusive job) and the 12 run jobs below (4 chains of 3 x 1200 CPU s)
Quest: .campaign/xapiand/quests/fuzz-cpu-hour.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code and doc committed in prospect `quest/xapiand/fuzz-cpu-hour` (head after `docs/perf/fuzz-targets.md`):
`4eea54e` cuts (fielddict cached bases, postings seeded-top-k size, segment third base 6000, replay.cc
REPLAY_COST), `tools/release_rows/fuzz_hour.sh` (build, cost, run with a CPU-time budget, corpus kept in
~/.scratch/fuzz-corpus/T, segment -max_len=256 -len_control=0). Binaries `before` (61e09a6) and
`4adadb9d82e8` built (jobs 261113-080553-565c57, 261113-081405-ed9126). Lead's `--long` is refused for
evidence ("a --long job proves no criterion"), so each decoder's hour is three chained jobs of 1200 CPU s
(light, 28 min bound, FUZZ_SEED=1..3), the corpus carried across them:

- segment (A2, A3) 261113-081451-5358c3 261113-081455-3f4ada 261113-081459-02662d
- postings (A2) 261113-081502-e083e7 261113-081506-55d102 261113-081510-b10410
- values (A2) 261113-081514-9eda0f 261113-081517-5a8e7e 261113-081521-9f4bc1
- fielddict (A2) 261113-081524-15b135 261113-081528-c7f8ca 261113-081531-084b60

All run after the cost-after job 261113-081415-b2fb3a (strict; cost-before is 261113-081412-4637f9).

## Next

1. When the cost jobs finish: put their table (ms per input, before/after, under the fuzzer binary) into
   `docs/perf/fuzz-targets.md` beside the replay table.
2. When the 12 runs finish: add each decoder's CPU seconds, executions, new units, peak RSS, corpus size,
   findings to the doc (`fuzz_hour.sh run` prints a summary line); a finding is minimized, recorded, reported.
3. Paste `campaign brief`'s table below, write Result, State "ready for review", push the journal.

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
