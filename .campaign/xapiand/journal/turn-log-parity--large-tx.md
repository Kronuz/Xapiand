# turn-log-parity--large-tx: journal

State: working
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Design and first code of A3 (strata worktree ../strata-wt/xapiand/turn-log-parity--large-tx):
a transaction whose command passes `command_bytes` streams as part records (first, middle, last)
of a new payload kind; applied and V move only at the last part; replay buffers a run and
abandons one a later record or the log's end interrupts, moving applied past it.

## Next

1. index.hh: `log_state::join` takes a part; `join_log` streams parts; `replay_log` runs; the
   `command_bytes` refusal goes (functions changed: `apply`, `log_state::join`, `join_log`,
   `replay_log`).
2. test/test_large_tx.cc: admission (logged admits what unlogged admits, past 64 MiB), the crash
   sweep over a multi-part transaction and a kv-shaped batch past 1 MiB, V held to the last part.
3. Mutants: limit kept, apply per chunk, V at a non-last part, abandoned run leaves applied.

## Blocked on

lead: msg-202, the file set lacks command.hh (the part payload), standalone_turn_log_test.cc
(T8 asserts the refusal A3 removes), the command fixtures and fuzzer, kv_strata.hh with its
batch test (the 1 MiB split), and docs/mechanisms/. Work in index.hh and test_large_tx goes on.

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
