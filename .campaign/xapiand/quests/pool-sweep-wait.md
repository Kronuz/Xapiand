# pool-sweep-wait: The sweep-merge pool test waits on a signal, not a 10 s loop

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status pool-sweep-wait`.

## Outcome

`test_the_sweeps_close_syncs_a_merges_output` (tests/test_pool.cc:2180-2210) failed `REQUIRE(unsynced)`
once under host load ~50 on 16 CPUs (job 261111-142834-9c25fd): it polls for a merge to finish within
a fixed 10 s. It waits on the merge's own completion instead, so load changes its duration, never
its verdict; what it checks is unchanged.

## Acceptance criteria

- A1 The test passes 50 times in a row under a light job running beside a CPU-burning load of twice the CPUs, with the build-test check green: jobs
- A2 [artifact] The wait names the event it waits on: `tests/test_pool.cc`

## Contracts

None.

## Decisions

- Lead, 11-12: from hunt row 85.

## May land early

Everything.
