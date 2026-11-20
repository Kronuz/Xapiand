# v2-replay: journal

State: working
Quest: .campaign/xapiand/quests/v2-replay.md (the lead's; never edit it)

Code: agent-campaign worktree /Users/kronuz/code/Kronuz/agent-campaign-wt/xapiand/v2-replay,
branch quest/xapiand/v2-replay, `tools/replay/` (plain Python 3, ruff-clean). Outputs and caches:
~/.scratch/xapiand/v2-replay/ (never the job store). Window: 14 days ending 2026-11-20T02:39Z
(this quest's start), `sources.SINCE/UNTIL`.

## Doing

Nothing. All six docs committed on quest/xapiand/v2-replay (not pushed, per the lead).

## Next

Lead: review and land. Open: the simulator's hour-by-hour queue error (77%); the fit turns slot
serialization off although the record shows slots serialized, so v2's worker has a dispatch or
batching rule the model lacks.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `tools/replay/`, `docs/v3/replay-inventory.md` (agent-campaign:57ad2a0) |
| A2 | `docs/v3/where-time-went.md` (agent-campaign:32c56ed; need section agent-campaign:0e84206) |
| A3 | `docs/v3/policy-simulation.md` (agent-campaign:0bed924) |
| A4 | `docs/v3/recording-gaps.md` (agent-campaign:1b47800) |
| A5 | `docs/v3/tool-cost.md` (agent-campaign:0de0bbe) |
| A6 | `docs/v3/leftovers.md` (agent-campaign:1b930c9) |

## Result

`tools/replay/` (replay.py: inventory, account, need, calls, toolcost, leftovers, simulate) and
docs/v3/{replay-inventory,where-time-went,policy-simulation,recording-gaps,tool-cost,leftovers}.md.
Top causes: active quests with nothing moving them 941 quest-h; blocked 746; idle light slots
with work ready 601 slot-h; machine waits 562 quest-h; failed runs 102 machine-h; land refusals
125 in 86 episodes, 68 h; runs no question needed 39 h (12 identical). Simulated: driver saves
794 quest-h; batching cuts drains 60 to 6 h; need saves 39 machine-h; all combined 923 quest-h.

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
  file set, top-level build files). "Criterion already evidenced" is
  not determinable: a criterion is many rows and v2 records no verdict per criterion over time.

- simulator calibration fits light slots, drain turn and slot serialization to the hourly queue;
  every policy runs on the fitted machine (a combined row had run on defaults, fixed at
  agent-campaign:40d42a2).
- Incident, mine: the toolcost pass's `status` started a worker on the store copy (CAMPAIGN_JOBS
  inherited), pid 81798, 19:39 to 22:20 PST. It ran 56 copied jobs for real (4.6 machine hours,
  exclusive holds included) and registered 14 git worktrees under the copy in 9 repositories.
  Stopped it and its 3 runners, removed those worktrees (`git worktree remove --force`, paths
  under ~/.scratch/xapiand/v2-replay/jobs-copy only), deleted the copy; run list kept in
  ~/.scratch/xapiand/v2-replay/rogue-worker/runs.json. Guard: agent-campaign:b9609c9. The real
  worker (58801) and store were not touched. Outputs those runs wrote outside the copy (bench
  OUT dirs under ~/.scratch) are not tracked and were left.

## Messages answered

- Lead, 2026-11-20 ~05:05Z (need, size and reuse): done in `tools/replay/need.py` and the
  simulator's need policy; the "criterion already evidenced" case is not determinable (rulings).
