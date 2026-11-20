# v2-replay: journal

State: waiting on jobs 261119-185505-c6555e 261119-190106-b2a89d 261119-190230-f45131 261119-191234-d03843 261119-191243-1be71a 261119-191244-d02997
Quest: .campaign/xapiand/quests/v2-replay.md (the lead's; never edit it)

Code: agent-campaign worktree /Users/kronuz/code/Kronuz/agent-campaign-wt/xapiand/v2-replay,
branch quest/xapiand/v2-replay, `tools/replay/` (plain Python 3, ruff-clean). Outputs and caches:
~/.scratch/xapiand/v2-replay/ (never the job store). Window: 14 days ending 2026-11-20T02:39Z
(this quest's start), `sources.SINCE/UNTIL`.

## Doing

Order from the lead (2026-11-19): A1, A2, then A5 and A6, then A3 and A4. All code is written and
committed (agent-campaign quest branch, head 91c8353); six jobs are queued (light queue ~33 deep,
4 light slots running at 19:22, so hours):
- c6555e `replay.py --refresh inventory` (A1): parses both lead transcripts (~0.9 GB), caches
  lead-<campaign>.json and usage-<campaign>.json, writes timeline.jsonl, inventory.json.
- b2a89d `replay.py account` (A2), after c6555e: account.json, reruns.jsonl, episodes.json,
  quests.json, hourly.json (1,669 `git diff` pairs, cached in diffs.json).
- f45131 `replay.py --refresh calls` (A5): every campaign/quiet call in lead and agent transcripts
  (3.7 GB of subagent logs), latency per call.
- d03843 `replay.py toolcost` (A5): read-only commands on a copy of the store's records, cProfile of
  status and brief, SQLite/log/dir-scan prototype of the hot path.
- 1be71a `replay.py leftovers` (A6), after b2a89d; d02997 `replay.py simulate` (A3), after b2a89d.

Preliminary numbers, run locally from the job records alone (`account.machine_time`, `waits`,
`light_capacity`; reproduce with the job's command): 496 job-run hours in the window (xapiand 277,
detent 219); light slots 17.2% used (461 of 2,688 slot-hours); exclusive jobs drained 47.3 h wall
for 19.7 h of exclusive running; idle light slot-hours while light work was ready: 288 no recorded
reason, 143 exclusive draining, 79 exclusive running, 67 build slot held, 25 strict beside;
exclusive median wait 55 min (p90 268 min), light 7.7 min (p90 66). document-gate alone submitted
133 exclusive jobs: 25 done, 48 cancelled, 31 skipped, 23 still queued.

## Next

1. When c6555e and b2a89d finish: `replay.py report inventory` and `report account` print the
   tables; write docs/v3/replay-inventory.md (A1) and docs/v3/where-time-went.md (A2) with prose,
   every number citing its `query NAME` (account.py docstrings) and the command.
2. docs/v3/tool-cost.md (A5) from toolcost.json and calls.json.
3. docs/v3/leftovers.md (A6) from leftovers.json.
4. docs/v3/policy-simulation.md (A3) from simulation.json; check calibration (landings and hourly
   queue within 10%), and state the error if it misses.
5. Fill numbers into docs/v3/recording-gaps.md (A4, drafted at cb845fb) from account.json.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending: tools/replay/, docs/v3/replay-inventory.md |
| A2 | pending: docs/v3/where-time-went.md |
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
