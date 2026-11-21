# turn-log-parity--group-commit: journal

State: waiting on jobs 261121-101105-1c95d9 261121-101111-6888d0 261121-101116-d36e5e
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

## Doing

Fix committed and pushed: strata:2f236c94 on quest/xapiand/turn-log-parity--group-commit (base
strata:330bfd41). Mutant strata:017bb849 (the base's `turn_seal.hh` and `index.hh` with the new
test) at `refs/mutants/turn-log-parity--group-commit`, not pushed. CI run 44934094130 (its
`linux-tsan` job is the TSan gate for this concurrency change).

## Next

1. Read jobs 261121-101105-1c95d9 (test, A4), 261121-101111-6888d0 (mutant, `--fails-without`,
   A4), 261121-101116-d36e5e (suite), and CI run 44934094130's `linux-tsan`; paste the brief's
   Evidence table; State "ready to merge" when all pass.

## Blocked on

Nothing. No journal change needed.

## Evidence

| Criterion | Evidence |
|---|---|
| A4 | test 261121-101105-1c95d9 (queued); mutant 261121-101111-6888d0 (`--fails-without`, queued); suite 261121-101116-d36e5e (queued); TSan: CI run 44934094130 `linux-tsan` (queued) |

Local runs (light holds, not evidence): the new test against the base mechanism fails 4 checks
(below); at the fix it passes.

## Result

The cohort wait (D5) counts per writer (rocksdb-parity N3). `turn_sealer` counts the writers a
turn releases from `await()` and `await_sealed()` (`released_writers()`, each waiter marked with
the turn), and a released wait leaves a thread-local mark that the writer's next join reports
through `turn_sealer::joined()` (from `index.hh` `join_log`): one arrival whether the writer
opened the record, joined one another writer opened, or found it full. `wake()` no longer counts
arrivals, so a writer outside the cohort cannot end the wait.

`test/test_barrier_cohort.cc` now runs K writers on I indexes over L loops, counted per writer,
24 commits each over 64 ms faultfs barriers (after 3 settling rounds every commit must ride
exactly the next barrier; writers that think 1/8-3/4 of a barrier must ride at most the barrier
in flight and the next). Local counts, base (group count) against fix:

| K, I, L | base: commits on a 2nd barrier, turns | fix |
|---|---|---|
| 4, 4, 1 | 0 of 84, 25 | 0, 25 |
| 4, 2, 1 | 9 of 84, 29 | 0, 26 |
| 8, 4, 1 | 8 of 168, 29 | 0, 26 |
| 8, 2, 1 | 71 of 168, 40 | 0, 26 |
| 2, 1, 1 | 8 of 42, 30 | 0, 26 |
| 4, 4, 4 | 0 of 84, 100 | 0, 100 |
| 8, 8, 8 | 0 of 168, 200 | 0, 200 |
| 4, 2, 2 | 26 of 84, 68 | 0, 51-52 |
| 4, 4, 1 think | 0 on a 3rd, 49 | same |
| 8, 4, 2 think | 0 on a 3rd, 98 | same |

So writers sharing an index ride one barrier per commit instead of up to two: at K=8 on two
indexes, 26 barriers for 24 rounds against 40 (1.54x fewer). Writers on their own index (every
`bench/kv` row: one index per thread) were already at one barrier; their mechanism is the same
count, so no bench row is expected to move, and no throughput claim is made here; the quest's
"When done" K = 2, 4, 8 rows on one log and a log per core are the lead's deciding numbers.
Doc: strata `docs/design/rocksdb-parity.md`, D5 paragraph (counts above).

## Rulings I made

- A4 vs D7 (rocksdb-parity task 5): A4 does not subsume D7. A4 bounds the barriers a waiting
  writer rides (the turn's timing against arrivals); D7 bounds the bytes a log admits and packs
  several records of a group into one turn (`turn_records`, the `ack=none` window). They share
  `log_state::join` and `seal`, but neither test proves the other: task 5 stays its own task.
- "Arrives during a barrier" is read two ways and both are tested: a writer the last barrier
  released (the closed loop; it must ride the next barrier, which the cohort wait gives), and a
  writer that joins while a barrier is in flight (think mode; at most that barrier and the next).
  The second held at the base too: it is an invariant check, and the base (group count) is the
  mutant for the first.
- A writer whose join finds the open record full reports its arrival before it waits in
  `await_sealed()`: the full record seals next turn whatever it does.
- The thread-local mark names its sealer by address and the turn; a sealer destroyed and another
  built at the same address could take one stale arrival, ending one cohort wait early (at most
  1/16 of a barrier lost). Not guarded.
- The test keeps its name and CMake registration; configurations replace its single K argument
  (`test_barrier_cohort K I L think` runs one). K=8 with two writers per index is the default's
  widest shared case; four writers per index was left out of the default to keep the serialized
  applies inside the 4 ms bound under TSan.
