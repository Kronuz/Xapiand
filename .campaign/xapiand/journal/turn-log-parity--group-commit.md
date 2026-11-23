# turn-log-parity--group-commit: journal

State: waiting on jobs 261121-213706-2e8246 261121-213641-0cfcd2 261121-213700-e680a2 261121-213646-d951e0 (head strata:6c3d0862)
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

## Doing

Head strata:6c3d0862 on quest/xapiand/turn-log-parity--group-commit, pushed (CI runs at it).
strata:80f91fd0 is already in the quest by git merge (strata:3b8da146); 6c3d0862 touches only
`test/test_barrier_cohort.cc`, lines the quest's merge left alone.

## Next

1. Read 2e8246 (test, A4), 0cfcd2 (mutant, `--fails-without`: log must show the build, then
   `std::logic_error ... nothing to write`), e680a2 (probe, must pass), d951e0 (suite) and the
   CI run at 6c3d0862; paste the Evidence table; State "ready to merge" at 6c3d0862 when all pass.

## Blocked on

Nothing. For the lead: quest head strata:b533db12 fails CI on every platform at build,
`test/test_reopen_damaged.cc` uses `faultfs::` (main's `fl::` rename; tolerant-open's file, not
this task's). CI run 44976323378.

## Evidence

| Criterion | Evidence |
|---|---|
| A4 | test 261121-213706-2e8246 (queued); mutant 261121-213641-0cfcd2 (`--fails-without`, queued); probe 261121-213700-e680a2 (queued); suite 261121-213646-d951e0 (queued) |

Read this session (at strata:80f91fd0): test 261121-151521-37deb9 passed; mutant
261121-151523-00cd16 (strata:191551bf, groups not writers) built, then failed 4 named CHECKs
(`test_barrier_cohort.cc:175`, K=4 I=2 L=2), as it must; suite 261121-151525-9704d9 timed out
at 30 min: hazard_litmus took 478 s (95 s at 2f236c94) and four tests none of which uses
`turn_sealer` ran on, with host load 73 on 16 CPUs: load, not a hang; rerun with
`ctest --timeout 900` so a hang names itself. Earlier: 261121-101429-f8726f passed,
261121-101433-65f5c5 failed as it must (both at 2f236c94); suite 261121-101437-69bee8 exit 130
(interrupted); 261121-114248-d19804 suite passed at 2f236c94.

CI run 44960903080 at 80f91fd0: all green but freebsd-x86_64, where barrier_cohort aborted on an
uncaught `std::logic_error` in K=4 I=2 L=2 (fixed by 6c3d0862, below).

Failed, not evidence: 261121-101105-1c95d9, 261121-101111-6888d0, 261121-101116-d36e5e exited 1 at
configure (Makefiles tree in `build`, D168).

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

- FreeBSD abort at 80f91fd0: writers started beside the loop threads, so a first wait could
  see `served_` false and write an inline turn the loop's first turn had emptied, throwing
  "nothing to write" (main's race; strata:1755bfb8 fixes it on the quest). The test measures
  served turns, so it marks each sealer `served(true)` before writers start (strata:6c3d0862),
  rather than merging the quest (it would bring the quest's broken `test_reopen_damaged` build).
  Proof: mutant strata:3f4bc171 (80f91fd0 with loops started one barrier late) must throw; probe
  strata:7e380d50 (6c3d0862 with the same delay) must pass. Refs under `refs/mutants/`, unpushed.

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
