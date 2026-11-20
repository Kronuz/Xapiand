# queue-drain-throughput review: correctness

Commit: agent-campaign:ad0308431e2b5a8a7c176f36317aa0a625386a4c
Verdict: APPROVE
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. Six non-blocking findings
below; F1 and G1 are worth folding into the landing (a small fix and a journal edit).

## Evidence I relied on

- Suite 261120-004854-a85314 at ad03084 (286 tests, OK): A1 to A3's tests pass at the head. Accepted: it
  is the full suite at the exact commit.
- Fails-without 261119-201532-834039 (A1) and 261119-201535-340225 (A3), run at main 63cc071 with the
  fixed tests copied in: both fail on a value mismatch (`['000003']` against `['000002']`; `(180, 480, 60)`
  against `(480, 480, 0)`), not on a missing name. Accepted: the tests fail without the fix.
- Replay 261120-004855-db9849 (A4) at ad03084: ran and printed the before/after table.
- `scripts/quiet.py` is not in the diff (`git diff main...HEAD --stat`): quiet's admission rules are
  unchanged, so crash behavior of holds and slots is quiet's, which this quest does not touch.

## What I checked myself

Scripts in `.scratch/xapiand/review-queue-drain-throughput-correctness/`, run as jobs at ad03084:

- 261120-011300-dc9f30 (`p1_starvation.py`): four queue shapes through `tests/queue_sim.py` on main
  and on the quest: a steady light stream with exclusive jobs among it (S1), 34 exclusive jobs of
  30 minutes with light arrivals (S2), a 6 h light flood with three exclusive jobs (S3), 34 exclusive
  jobs with builds and no light work (S4). Every job finishes on both; the quest is no worse than main
  on any wait; the longest exclusive wait under the flood is 40 min (main 36); the longest light wait
  behind the 17.6 h exclusive queue is 111 min (main 168).
- 261120-011526-56e765 (`p5_s1_timeline.py`): why S1's exclusive waits reach 196 min: each drain waits
  its 20 minutes, then the longest running light job (up to 45 min), and an hour-long turn ends before
  the next one; every exclusive job gets its hold, the waits are the design's, main's are the same.
- 261120-011301-b35473 (`p2_charge.py`): `charged`, `queue_health`, `add_charge` and `idle_for` called
  directly with edge records (F1, F2, F3 below; the idle check beside a charged job is right both
  sides of the boundary).
- 261120-011302-572b89 (`p3_env.py`): `mirror_env` with a PATH that leads with the shim (F4).
- 261120-011418-8c04b1 (`p4_attribution.py`): main, main with only the fair-share rounding, and the
  quest, over the three replay shapes (F5).

Answers to the four questions:

1. Starvation. Not found, either way. An exclusive job is bounded by `drain_wait` (20 min) plus the
   longest running light job: once overdue, `plan` admits only light work that ends before the running
   work, so the supply of new work stops. `yielded` and `full` carry no state: `batch_of` ends a batch
   once any light job was claimed after the heavy one ended, so after a full batch light work goes
   first once and the drains behind it start when it ends; they cannot alternate forever. The
   `weight and not full` condition equals the old `not is_heavy and not is_bare` except while `full`.
   Light work behind a long exclusive queue: a batch is capped at 6 jobs or 30 min, then light goes
   first once, inside a turn too (S2, S3). "No light admission while an exclusive waits" is not in this
   diff: it is quiet's existing rule (`strict_beside`), now pinned by `ExclusiveIsolation`. The
   scheduler's own version of it was removed in d768ed1 (see G1). quiet's 10-minute starving rule can
   still send a light request ahead of a waiting exclusive one; unchanged, and bounded by the same cap.
2. Crash or restart. `plan` is a pure function of the job records: `batch`, `full` and `yielded` are
   derived, never stored, so a restarted worker (or `worker --restart`) recomputes the same decision.
   The only new persistent state is `charge`, written atomically after each sample; a restart skips
   one interval (`since` is 0), so nothing is counted twice. Holds and slots are quiet's and
   `admit`/`release`, both untouched. The one inconsistency found is F1.
3. Charged slots. Declared and measured both count (the greater), the idle check compares the waiting
   job's charge with the free slots on both sides of the boundary, and a job charged 4 of 8 slots
   blocks a 5-slot job and admits a 4-slot one (p2 part d). Defects: F1 and F2.
4. Harness. The strips are right for the tests, and they do hide something the tool can fix: F4.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | defect | A requeued attempt (`--retries`, reaped after a lost runner) keeps the old `charge` file: `reap` rewrites `state.json` and removes `claim` and `admitted`, not `charge`. `charged` divides the old attempts' slot-seconds by the new attempt's shorter span: a 10-minute second attempt after a 1 h attempt at 4 slots is charged 19.0 slots of 8, and one such job makes a 10-minute run count 190 slot-minutes in `queue_health`. Scheduling is not affected (it uses the live meter), only the health lines and the idle count. `charged` is also unbounded: nothing caps it at the slots. | 261120-011301-b35473 part a; `scripts/campaign_tool.py` `reap` (requeue branch), `charged` | Remove `charge` where `reap` requeues, and `min(..., light_slots())` in `charged`. Test: `queue_health` for a record carrying a charge larger than its span allows stays at or under capacity; a requeued job's `charge` file is gone. |
| F2 | no | nit | `charged` takes `max(declared, 1 + measured excess / duration)`, but the meter's excess is counted over one slot, not over `max(declared, measured)`: a job declaring 3 slots that measures 4 for half its run reports 3.0, the mean of what it held is 3.5. Under-reports only a `--cpus` job that also exceeds its declaration. | 261120-011301-b35473 part b | Leave, or have the worker add the excess over `max(declared, measured)`. |
| F3 | no | nit | `add_charge` recreates a missing job directory (`atomic_write` makes parents), leaving a directory holding only `charge`. Needs a running job's directory pruned between the worker's snapshot and the sample; `job prune` only takes finished ones, so not reachable today. | 261120-011301-b35473 part c | Skip when `(d / "state.json")` is missing. |
| F4 | no | gap | The harness strips are not only test hygiene: the tool leaks the same variables. (a) `mirror_env` finds the real git with `shutil.which("git", PATH)`; a job submitted from inside a job records a PATH that leads with the shim (`cmd_job_submit` stores `os.environ["PATH"]`, `_run` applies it), so `CAMPAIGN_GIT` becomes the shim, which then calls itself: the hang in the journal's 593d3f. (b) `spawn()` gives `Popen` no `env=`, so a worker started from inside a job or a quiet hold carries that job's `CMAKE_BUILD_PARALLEL_LEVEL`, `CTEST_*`, `GIT_CONFIG_*`, `CAMPAIGN_GIT` and `QUIET_HELD` into every runner; `_run` copies `os.environ`, so `setdefault` for a light job keeps the stale value and a heavy job inherits a light job's 4-way setting. (b) is by code reading (`scripts/campaign_tool.py` `spawn`, `_run`'s `env = dict(os.environ)`), not run: a probe needs a worker. The harness change is still correct, since the suite is a job and its tests start their own workers. | 261120-011302-572b89 (a: the shim is its own `CAMPAIGN_GIT`); `spawn`, `mirror_env`, `_run` | A follow-up quest, not this one: `mirror_env` skips its own shim directory when resolving the real git, and `spawn` for the worker starts from a scrubbed environment (drop `JOB_*`, `QUIET_HELD`, `CAMPAIGN_GIT`/`PYTHON`/`TOOL`, `GIT_CONFIG_*` entries, the shim on PATH, light-hold build settings). Tests: a worker spawned under those variables hands its runners none of them. Keeping the harness strips as they are. |
| F5 | no | gap | On today's shape queued at once, the quest's A4 shape, the whole replay gain is the fair-share rounding: main with only the two rounding lines gives the same numbers as the quest (28.2% light use, 212 idle slot-minutes, 277 drain minutes, 4.8 drains, 7.23 h). The batching and turn rules move only the recorded 12 h queue (42.0% to 46.3%, drain minutes 384 to 249), and there drains rise from 13.0 to 14.0 (the journal's 11.0 was the earlier code). On today's shape drain minutes go 281 to 277: the 34 exclusive jobs' own 238 minutes bound them. A1's "run back to back inside one drain instead of a drain each" is therefore shown by the unit test, not by a replay count. | 261120-011418-8c04b1 | Say so in the journal (the journal already attributes today's shape to rounding); no code change. |
| G1 | no | gap | The journal's Result no longer describes the code at ad03084. Items 1 and 2 (a drain admits no light job or build while it waits or runs; the next exclusive job joins a finished batch at once) were removed in d768ed1; the replay table is the local a3c1939 run (28.4%, 229, 11.0 drains), not db9849's (28.2%, 212, 14.0 on the recorded queue); "the two existing replay tests now use queue_sim" is wrong (they are main's, unchanged; only the CLI uses it). A4 asks for the numbers recorded in the journal. The quest's Outcome still says a drain "admits no new work it would have to wait for": at the head that holds for an overdue drain (existing rule) and by quiet, not for a drain still waiting its first 20 minutes. | `campaign get queue-drain-throughput journal`; `git log` d768ed1; db9849's log | Replace Result items 1 and 2, paste db9849's table (the lead's own Next 1), and have the lead rule whether the Outcome sentence stands. |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit.

## Not run

- No worker on a real or copied store: the crash and restart answers come from reading `plan`,
  `batch_of`, `reap` and `cmd_worker` and from the pure-function probes, not from killing a worker.
- No Linux or FreeBSD run: `ExclusiveIsolation` was seen passing only in the macOS suite job; on Linux
  `strict_set` can differ, which the test's exclusive path should not depend on, but I did not run it.
- The simulator is the contributor's model of quiet's grants; I checked it against `try_hold`'s rules
  by reading, not against a live host.
- F4 (b) and the `QUIET_HELD` carry-over are code reading only.
- CI at ad03084 was still running when I started.
