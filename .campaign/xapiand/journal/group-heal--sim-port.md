# group-heal--sim-port: journal

State: working
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

## Doing

Sizing `sim_run` so its three CTests stay short under ASan (probe job 261120-175321-108824).
At the default size (8 keys, 200 ms) seed 7 checks in ~17 s (2.2M search nodes, 92 unknowns).

## Next

1. Pick `--keys/--ms` defaults from the probe; fix sim_run.cc's long header line.
2. Jobs: the sim suite at head (standalone and in strata's build), the lost-write mutant
   (`--fails-without`), determinism (simulation_run_same_history).
3. Push the task branch.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| sim tests at head | pending |
| planted fault rejected | pending |
| one seed, one history | first run: job 261120-174845-dcee0c at strata:4a26c2dd, seed 7 twice, sha256 cd83739f... both (cancelled after; tests 8 and 9 too slow) |

## Result

strata `sim/` (engine, clock, random, network, history, checker, sim_check) ported verbatim from
`design/simulator` at strata:27cfb303 (strata:7aca130b); wired into strata's build
(strata:4a26c2dd); `sim_run` end to end; `docs/mechanisms/simulator.md`.

## Rulings I made

- Port only `sim/`; the design doc and reviews stay on `design/simulator` (outside the file
  set); the mechanism note cites them and lists the open review findings (SIM-R7 residual,
  R20 to R22, R25's mutation classes) for `sim-rows`.
- SIM-R25: `-Wall -Wextra -Werror` as private options on the simulator's own targets.
- Root CMakeLists.txt: `set(SIMULATION_TESTS ON)` + `add_subdirectory(sim)` at the end of the
  top-level block, as `design/simulator` had it.
- The "planted fault" proof is `sim_run --plant lost-write`: a register server that acks one
  put without storing it; the checker must reject. Its fault is in the system under test, not
  the checker, so the mutant job runs the planted binary and passes when it exits nonzero.
