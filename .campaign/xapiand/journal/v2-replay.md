# v2-replay: journal

State: waiting on jobs 261119-185505-c6555e 261119-190106-b2a89d 261119-190230-f45131
Quest: .campaign/xapiand/quests/v2-replay.md (the lead's; never edit it)

Code: agent-campaign worktree /Users/kronuz/code/Kronuz/agent-campaign-wt/xapiand/v2-replay,
branch quest/xapiand/v2-replay, `tools/replay/` (plain Python 3, ruff-clean). Outputs and caches:
~/.scratch/xapiand/v2-replay/ (never the job store). Window: 14 days ending 2026-11-20T02:39Z
(this quest's start), `sources.SINCE/UNTIL`.

## Doing

Order from the lead (2026-11-19): A1, A2, then A5 and A6, then A3 and A4.
- `replay.py inventory` (A1) as job c6555e: loads every source, parses the lead transcripts (heavy,
  cached as lead-<campaign>.json), writes timeline.jsonl and inventory.json.
- `replay.py account` (A2) as job b2a89d, after c6555e: writes account.json, reruns.jsonl,
  episodes.json, quests.json, hourly.json.
- `replay.py calls` as job f45131: every campaign/quiet call in lead and agent transcripts, latency.
- `replay.py toolcost` (A5): written; times read-only commands on a copy of the job store's records.

## Next

1. When c6555e and b2a89d finish: write docs/v3/replay-inventory.md and docs/v3/where-time-went.md
   from inventory.json and account.json, every number citing its `query NAME` in account.py.
2. Submit `replay.py toolcost` as a job; write docs/v3/tool-cost.md (A5) with calls.json.
3. A6 leftovers (disk by owner/age, zombies, duplicates); then A3 simulator, A4 gaps.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending: tools/replay/, docs/v3/replay-inventory.md |
| A2 | pending: docs/v3/where-time-went.md |
| A3 | pending |
| A4 | pending |
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
- Tool timings run with CAMPAIGN_JOBS at a copy of the store's records, since `status` reaps.
