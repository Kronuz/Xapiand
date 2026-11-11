# barrier-warning: journal

State: blocked on lead: barrier-warning-strata not on strata main; contract has two mechanisms for one switch (B1, B2)
Quest: .campaign/xapiand/quests/barrier-warning.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: blocked before any code (see Blocked on). Investigation only, no jobs run.

## Next

1. The lead rules on Blocked on (B1, B2); then, once a strata commit pinning completion >= e49f288 is on strata main: pin it (A3), add `--full-flush-ordering` (opts), a `PosixIOOptions` builder in `src/engine/strata/` used by `acquire` (shard.cc) and the catalog's `resolve` (pool.cc) that sets `full_flush_ordering` and a `barrier_warning` callback logging the volume (mount point, source) and the switch, a seamed test (A1, A2), options docs and changelog (A2, A4).

## Blocked on

Lead. Found at Xapiand:026dd9d0b, strata origin/main 71750940, completion origin/main 3a3c205:

- B1 Dependency not on strata main. `barrier-warning-strata` is `planned`, not started; strata main has no `full_flush_ordering` or `barrier_warning`, and still pins completion e2c64747, older than barrier-warning-completion's e49f288. Xapiand takes completion through strata (no completion pin of its own; CMakeLists pins rule), so `PosixIOOptions::barrier_warning` does not exist at Xapiand's pins. A3 cannot be met until a strata commit bumping completion lands on strata main.
- B2 The contract has two ways to set one thing. strata never builds a `PosixIO` (`git grep` on origin/main: none); hosts do. Xapiand builds every one itself: `acquire` in `src/engine/strata/shard.cc:450` (hostless) and the catalog's `host::resolve` in `src/database/pool.cc:571` (the server's path). completion's `PosixIOOptions` already carries `full_flush_ordering` and `barrier_warning` (completion:e49f288). A strata index option for the same thing would be a second mechanism that cannot reach an IO the host already built.
  - Option 1 (recommended): narrow `barrier-warning-strata` to the completion pin bump plus its A5/A6 (faultfs, calibration note), no index option and no strata callback; reword this quest's A2 to "reaches completion's `PosixIOOptions::full_flush_ordering` on every IO Xapiand builds"; add `src/database/pool.cc` (and `src/database/pool.h` if the builder is a member) to this quest's file set. A3 stays: pin that strata commit (and prospect's, if `harness/pins_check.py` needs prospect on the same completion).
  - Option 2: keep the strata index option as planned. strata would need its own ordering override on top of the host's IO, and Xapiand would set it in two places; not recommended (one mechanism per thing).
  - Either way `src/database/pool.cc` must join the file set: without it the switch misses the server's indexes.

## Evidence

No jobs run: nothing to build until B1 clears.

| Criterion | Evidence |
|---|---|
| A1 | blocked (B1) |
| A2 | blocked (B1, B2) |
| A3 | blocked (B1): strata origin/main 71750940 has no barrier-warning-strata commit |
| A4 | blocked (written with the code) |

## Fails without the fix

None yet.

## Result

None yet.

## Rulings I made

- R1 Switch name: `--full-flush-ordering` (a flag, default off: the ordering barrier stays the default, D10), the same name as completion's option, so the warning, the docs and the code say one thing.
