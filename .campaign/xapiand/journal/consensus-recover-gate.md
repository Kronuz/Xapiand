# consensus-recover-gate: journal

State: ready for review
Quest: .campaign/xapiand/quests/consensus-recover-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code round done at consensus:b6a604a (pushed); evidence jobs 261125-034814-cbfb1c (running), 261125-034822-6f0c97, 261125-034825-3c9017 (queued behind 85 jobs) and CI pending.

## Next

1. Lead: read the three jobs and CI; durability review. Before landing, merge consensus main if detent:consensus-inline-sync-loop landed (DRIVER.md only overlaps).

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

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261125-034822-6f0c97 (the four recover_gate cases at consensus:b6a604a); 261125-034825-3c9017 (`--fails-without`, mutant gate-after-start: lone "refused attempt 1: the row holds the recovered term" and follower "accepted, it recovers term 1" fail) |
| A2 | 261125-034814-cbfb1c (build-test); CI on the pushed branch |
| A3 | consensus docs/DRIVER.md "Recover gate" (gate, reason text), CHANGELOG.md 2026-11-25 at consensus:b6a604a |

Local iteration (not evidence): the mutant also fails recover_gate_accepts; recover_gate_closed
survives it, since a matching closed record resumes its term with no election, so Start changes
no state there. Under the mutant every other driver_test case passes.

msg (lead, Detent's answer): Worker::recover is unchanged (its signature and behavior; the gate is
only in Driver::recover, Worker gains the accessor retains_suffix). consensus main had not moved
past 217481b at 03:50; no merge needed yet.

## Result

consensus:b6a604a. `Driver::recover(id, restore, closed, accept, token)` and `recover_refused`
(include/consensus/driver.h), `Core::retains_suffix` public (core.h), `Worker::retains_suffix`
(worker.h). Tests in test/driver_test.cc (`recover_gate_lone`, `_closed`, `_follower`,
`_accepts`), mutant `gate-after-start` in test/driver_mutants.py. How it works: docs/DRIVER.md,
"Recover gate". Leaves open: nothing in consensus; strata-journal-behind consumes it.

## Rulings I made

- The gate runs synchronously inside Driver::recover, after Worker::recover and reopen and before
  any turn, not in admit() beside Start: typed init and the pre-Start reclaim then never run for
  a refused group (fewer writes than D7 allows).
- A refused row is `opened` and fenced, so `close` retires it through the existing fenced path
  (with `opened` false its retirement never ran and close hung). `fence_seen` is set: the call
  carries the refusal, so the Sink hears no Fenced and peers get no Gone.
- Byte equality is checked against a control: the same disk snapshot restarted with no group
  opened, since every host start begins a new turn-log segment
  (`l...-turns-000N`). The refused restart's files equal the idle restart's exactly.
- A second recover on a row already open: unchanged (X15). A refused row fails it with `fenced`;
  an accepted, started one completes it without rechecking its `accept`.
- Tests live in driver_test.cc (not a new file): CMakeLists.txt is outside the file set, and
  consensus_driver_test already builds with the mutant include.
