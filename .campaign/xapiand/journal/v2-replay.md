# v2-replay: journal

State: working
Quest: .campaign/xapiand/quests/v2-replay.md (the lead's; never edit it)

Code: agent-campaign worktree /Users/kronuz/code/Kronuz/agent-campaign-wt/xapiand/v2-replay,
branch quest/xapiand/v2-replay, `tools/replay/` (plain Python 3, ruff-clean). Outputs and caches:
~/.scratch/xapiand/v2-replay/ (never the job store). Window: 14 days ending 2026-11-20T02:39Z
(this quest's start), `sources.SINCE/UNTIL`.

## Doing

Per the lead (msg after the first handback): queued jobs cancelled; each pass runs under
`quiet light v2-replay-NAME 30 ...` in order (toolcost under `quiet strict`). Done: inventory
(A1 doc agent-campaign:57ad2a0), account (A2 doc agent-campaign:32c56ed). Now: calls, then toolcost,
leftovers, simulate.

## Next

1. `quiet light v2-replay-calls 30 ... replay.py --refresh calls`; then
   `quiet strict v2-replay-toolcost 30 ... replay.py toolcost`; write docs/v3/tool-cost.md (A5).
2. `quiet light v2-replay-leftovers ...`; write docs/v3/leftovers.md (A6).
3. `quiet light v2-replay-simulate ...`; calibrate; write docs/v3/policy-simulation.md (A3).
4. Fill numbers into docs/v3/recording-gaps.md (A4).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `tools/replay/`, `docs/v3/replay-inventory.md` (agent-campaign:57ad2a0) |
| A2 | `docs/v3/where-time-went.md` (agent-campaign:32c56ed) |
| A3 | pending |
| A4 | draft: docs/v3/recording-gaps.md (agent-campaign:cb845fb) |
| A5 | pending |
| A6 | pending |

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
