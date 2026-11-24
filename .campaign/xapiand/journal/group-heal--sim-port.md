# group-heal--sim-port: journal

State: waiting on jobs ci:macos-arm64, ci:linux-asan-ubsan, ci:linux-tsan (strata CI run 44853762979 at strata:6d870c05)
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

## Doing

Waiting on strata CI run 44853762979 at strata:6d870c05 (the task head, pushed): ci:macos-arm64
is the build-test gate (D57) and runs the sim suite; the sanitizers run it too.

## Next

1. CI green on every job: set State to ready for review. Red: fix, push, cite the new run.
2. Watch gcc: `-Wall -Wextra -Werror` on the sim targets is new to CI (SIM-R25).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| sim tests at head | ci:macos-arm64 (plus ci:linux-asan-ubsan, ci:linux-tsan), strata CI run 44853762979 at strata:6d870c05, pending. Local suite jobs 1bb70e and 28e256 cancelled per the coordinator's D57 correction: CI runs the same ctest |
| planted fault rejected | 261120-180342-65db6c done exit 0 at strata:6d870c05: seeds 1-5 linearizable; with --plant lost-write each a violation (keys 5, 1, 6, 5, 13), plain run exits 1 |
| one seed, one history | 261120-180334-1b82c5 done exit 0: seed 7 twice, sha256 8eacc64c... (132,121 B); seed 11 twice, sha256 75961460... (102,096 B) |
| sizing | probe 261120-232059-b57cfe: 16 keys/100 ms checks in <0.1 s per seed; 8 keys/200 ms took ~17 s (2.2M nodes) |

## Result

strata `sim/` (engine, clock, random, network, history, checker, sim_check) ported verbatim from
`design/simulator` at strata:27cfb303 (strata:7aca130b); wired into strata's build
(strata:4a26c2dd); `sim_run` end to end, sized at strata:6d870c05; `docs/mechanisms/simulator.md`.

Coordinator's D57 correction (build-test is ci:macos-arm64): applied; the duplicate local suite
job 261120-182237-28e256 cancelled too.

## Rulings I made

- Port only `sim/`; the design doc and reviews stay on `design/simulator` (outside the file
  set); the mechanism note cites them and lists the open review findings (SIM-R7 residual,
  R20 to R22, R25's mutation classes) for `sim-rows`.
- SIM-R25: `-Wall -Wextra -Werror` as private options on the simulator's own targets.
- Root CMakeLists.txt: `set(SIMULATION_TESTS ON)` + `add_subdirectory(sim)` at the end of the
  top-level block, as `design/simulator` had it.
- Default size 16 keys, 100 ms (probe above); 50 ms missed the lost write at one seed.
- The mutant job is a plain job requiring exit 1 (not `--fails-without`, which needs a quest
  criterion; this task's proof is no A-criterion of the quest).
- The "planted fault" proof is `sim_run --plant lost-write`: a register server that acks one
  put without storing it; the checker must reject. Its fault is in the system under test, not
  the checker, so the mutant job runs the planted binary and passes when it exits nonzero.
