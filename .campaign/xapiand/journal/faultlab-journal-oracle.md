# faultlab-journal-oracle: journal

State: waiting on jobs 261121-032543-4b16fb (first fixture generation at faultlab:f58b47d)
Quest: .campaign/xapiand/quests/faultlab-journal-oracle.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

faultlab worktree /Users/kronuz/code/Kronuz/faultlab-wt/xapiand/faultlab-journal-oracle, commit f58b47d: test/fixtures/{sector_sequences.h, memory_io_gen.cc}, sector_test reads test/fixtures/memory_io_states.txt, CMake fetches journal only with -DFAULTLAB_REGENERATE_MEMORY_IO=ON. Job 261121-032543-4b16fb runs the generator and writes the fixture to the scratchpad (memory_io_states.txt); the fixture is not committed yet.

## Next

1. Copy the generated memory_io_states.txt to test/fixtures/, commit, push.
2. Submit: suite (`campaign verify faultlab-journal-oracle --for A1`), regenerate-and-diff job (gen.sh to a scratch path, diff against test/fixtures/memory_io_states.txt) --for A1, and the A2 grep --for A2; also `--fails-without` is not needed.
3. Push the branch; A3 is CI.

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
