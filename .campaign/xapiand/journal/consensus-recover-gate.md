# consensus-recover-gate: journal

State: working
Quest: .campaign/xapiand/quests/consensus-recover-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lifecycle table (below) written; next the gate in Driver::recover.

## Next

1. Gate in Driver::recover (driver.h), Worker::retains_suffix (worker.h), Core::retains_suffix public (core.h).
2. test/driver_recover_gate_test.cc (A1 values), mutant gate-after-start in test/driver_mutants.py.
3. docs/DRIVER.md, CHANGELOG.md (A3); verify + mutant job.

## Lifecycle: recover to Start

The gate runs in `Driver::recover`, synchronously, right after `Worker::recover` (and
`Worker::reopen` when `closed` matches) and before `opened` is set: no turn has run, so neither
`run_one` nor `admit` (Tick + Start) can reach the group. A refusal fences the Worker
(`fence_reason` = the reason) and fails the call with `recover_refused`; `opened` stays false, so
`step` only moves the row on `close`, and the fenced Worker writes no close record
(`close_store` returns when fenced; the Store ends and the host releases the group).

| Path | What runs before the gate | Persists before the gate | Between gate and Start (accepted only) |
|---|---|---|---|
| plain log (turn log or own) | `store_.recover` replays; Core from `recovery.finish` | nothing consensus-visible (journal recovery only) | inventory turns, Tick + Start |
| closed record (`closed` given) | decode (no IO), `store_.recover`, `Core::reopen` (in memory: `resume_`, `term_record_`) | journal reopens the table entry and stages its checkpoint record (journal.h:604-610, D7/Q1: bytes change, state equal) | the term record persists after Start |
| checkpoint plus tail | `store_.recover` restores the bundle (`restore` callback reads the application image), replays the tail | nothing (the restore reads) | as plain |
| bootstrap-only typed init | `store_.recover` finds the generic bootstrap; `initialization_` queued, Core from `initial_state()` | nothing: the typed record is written by a later turn, which a refused group never runs | typed init persists in the first turns; `ready()` waits for it |
| pre-Start reclaim (store above headroom) | nothing | nothing: reclaim runs in `run_one`, after the gate | reclaim; Pressure after a full pass fences `over_headroom` |
| store above headroom | nothing | nothing | `over_headroom` fence before Start (driver.h:803) |
| empty `accept` | no gate | as today | as today |

So on every refused path the only write is the journal's own table reopen of a closed group
(D7 first bullet); the typed init and the reclaim (D7's other two) do not run at all.

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
