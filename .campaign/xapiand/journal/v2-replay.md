# v2-replay: journal

State: working
Quest: .campaign/xapiand/quests/v2-replay.md (the lead's; never edit it)

Code: agent-campaign worktree /Users/kronuz/code/Kronuz/agent-campaign-wt/xapiand/v2-replay,
branch quest/xapiand/v2-replay, `tools/replay/` (plain Python 3, ruff-clean). Outputs and caches:
~/.scratch/xapiand/v2-replay/ (never the job store). Window: 14 days ending 2026-11-20T02:39Z
(this quest's start), `sources.SINCE/UNTIL`.

## Doing

Calibrating the simulator (A3). First fit (light slots only): landings 233 vs 237 recorded (1.7%
off), median wait 8.4 vs 9.4 min, hourly queue 19.0 vs 11.5 mean (133% abs error). The excess
sits on single-slot quests (logger-exit-lifetime, reader-s6). Fixed an exclusive-batching
deadlock (agent-campaign:823c33a); now fitting drain turn and slot serialization too
(agent-campaign:10c3089). Running `quiet light v2-replay-need`, then simulate.

## Next

1. Write docs/v3/policy-simulation.md (A3) with the calibration error stated, the need policy's
   machine-hours and quest-hours, and the A2 need section's placeholders filled.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `tools/replay/`, `docs/v3/replay-inventory.md` (agent-campaign:57ad2a0) |
| A2 | `docs/v3/where-time-went.md` (agent-campaign:32c56ed) |
| A3 | pending |
| A4 | `docs/v3/recording-gaps.md` (agent-campaign:e386e06) |
| A5 | `docs/v3/tool-cost.md` (agent-campaign:0de0bbe) |
| A6 | `docs/v3/leftovers.md` (agent-campaign:1b930c9) |

## Result

pending

## Rulings I made

- Window is fixed at this quest's start minus 14 days so a rerun of the replay gives the same
  numbers; the job store only reaches back to 2026-11-05 15:05 (pruned at 14 days) and state history
  to 2026-11-08, so the first ~3 days of the window lack quest status.
- Land refusals come from the lead's transcripts (and Claude Code background task outputs, kept
  from 2026-11-15 only): v2 records no refusal.
- A rerun is a later job of the same shape (repository, command, directory, level, build,
  environment); its necessity is decided by `git diff` between the two commits against, in order,
  docs-only, the paths the command names and build files, then the quest's file set (no read sets
  exist: reads.json is absent from every job).
- Tool timings run with CAMPAIGN_JOBS at a copy of the store's records, since `status` reaps; the
  copy goes file by file (rsync's walk visits the slots' millions of files).
- No queue-jumping: jobs keep the default quest priority (1) though the light queue is ~33 deep.
- Not pushed: the lead's prompt says don't push.
- Rerun shapes mask commit ids and `$$` in commands: commands embed them in output paths, which
  made every document-gate resubmission a new shape.
- The document-gate 23 reruns count necessary: `bench/kv/run.py`, which they run, changed (3 lines,
  a new workload set). File-level input identity would rerun them too; recorded in A2.
- leftovers: zombies resolve `state.json` worktree paths against the home (the first pass read
  them against the cwd and flagged all 33 in-flight quests); starved exclusive jobs are reported
  apart from zombies, since nothing marks them stuck.
- need categories (one per job, first that applies): identical rerun; quest landed or dropped
  before it began; superseded by a newer submission of the same row at another commit before it
  began; trial past the first two of a passing row at one commit (an upper bound: no threshold is
  recorded, so whether the interval had cleared is unknowable); landing-time rerun (after the
  first accept or refusal) whose changed paths miss the declared-input proxy (named paths, quest
  file set, top-level build files and build files under those). "Criterion already evidenced" is
  not determinable: a criterion is many rows and v2 records no verdict per criterion over time.

## Messages answered

- Lead, 2026-11-20 ~05:05Z (need, size and reuse): done in `tools/replay/need.py` and the
  simulator's need policy; the "criterion already evidenced" case is not determinable (rulings).
