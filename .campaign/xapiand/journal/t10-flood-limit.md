# t10-flood-limit: journal

State: blocked on lead: the over-slice charge lives in Kronuz/task (outside the file set); prototype jobs running
Quest: .campaign/xapiand/quests/t10-flood-limit.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 test written (tests/test_loop_yield.cc, an `overrun` seam mode; it needs the task fix to pass). Prototype of the fix as a
scratch clone of Kronuz/task at the pin 18eb516 (`/Users/kronuz/.scratch/t10-flood-limit/task-proto`, branch proto-overslice,
`checkpoint()` adds max(1, ceil(turn/slice)) to spent_): build job 261117-010341-e6d5af, then strict sweep job
261117-010442-1c2b3f (1 reactor, 1/2/4/8/16 clients) to see whether the premise holds before a task change is proposed.

## Next

1. Read the sweep (`/Users/kronuz/.scratch/t10-flood-limit/sweep-proto.log`) against master's 22/24/47/87/134 ms.
2. Lead rules on the Blocked-on items; then A3 (lua_bounds.py derived bound), A4 docs, A5 suite.

## Blocked on

1. File set. `budget::checkpoint()` counts a spent slice (`++b.spent_`, Kronuz/task `task.hh`, pinned at 18eb516 in CMakeLists.txt:766).
   No Xapiand file in the set can charge more than one: `budget` has no charge API (`rearm_light` sets, never adds).
   Options: (a) a Kronuz/task change (checkpoint charges ceil(turn/slice); a library test there) and a repin in CMakeLists.txt
   (add it to the file set); (b) none inside Xapiand. Recommendation: (a), after the prototype confirms the premise.
2. Premise to check. Each flood request's one 22 ms call runs in its first turn, which is light (a request starts light, D2 f),
   so the over-slice charge lands after the stretch: a GET queued in the light FIFO still waits behind every flooder whose
   request is in its first turn. The charge may not flatten p99; the prototype sweep decides.
3. Side effect: the charge reads wall time, so a turn descheduled for N slices is charged N (D1 already notes
   oversubscription misclassing; this widens it). Tests that assumed +1 per spent slice (the chunk lane check) now compare lane
   to `spent() >= light_slices`.

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
