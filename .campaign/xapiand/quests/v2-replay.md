# v2-replay: Replay v2's records: where every hour went, and what policies would have done

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status v2-replay`.

## Outcome

A replay of campaign v2's recorded history accounts for where machine time, wall time and agent tokens went, and simulates alternative orchestration policies on the same history, so the v3 design is chosen by numbers. Owner, 2026-11-19: orchestration must be fully deterministic; v2 is flaky, inconsistent, over-engineered and slow; v3 must also record enough for a v4 to repeat this analysis. Owner, same day: every command answers in milliseconds; nothing loose (no missing or zombie quests or jobs, no duplicate work, no unknown leftovers wasting disk); no wasted time, quota or CPU; the critical path stays focused and optimal yet flexible when scope or direction changes; the implementation language is an open question to settle with numbers.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 [artifact] `tools/replay/` loads every v2 record available on this machine (job specs and states under ~/.scratch/campaign/jobs, each campaign's state.json and its history, git logs of the home and quest repositories, journals, efficiency.md, quiet's records) into one event timeline, and `docs/v3/replay-inventory.md` lists each source, its span, its counts and what it cannot tell (fields never recorded)
- A2 [artifact] `docs/v3/where-time-went.md` accounts for the xapiand and detent campaigns' last 14 days by cause, in hours and as shares: job run time by level; exclusive and drain waits; reruns split into necessary (an input changed) and avoidable (none did, measured by diffing the paths the job's command and build read, or the quest's file set when that is unknowable); land refusals and the CI rounds and re-accepts they caused; quests with nothing moving them (no job queued, no agent turn, every dependency landed); agent turns and tokens per landed quest where recorded. Every number cites its query.
- A3 [artifact] `docs/v3/policy-simulation.md` replays the same arrivals under at least: v2 as recorded (calibration: the simulated landings and queue lengths within 10% of recorded), input-scoped identity, a merge queue that tests the merged result once, exclusive batching, and a deterministic driver that resumes, reviews and lands with no lead delay; alone and combined, each with landings per day, machine hours and agent tokens.
- A4 [artifact] `docs/v3/recording-gaps.md` lists what v3 must record so a v4 can rerun this analysis without inference (why each job was submitted, its read set, each agent turn's cost and outcome, each wait and its cause), with the field each gap would have needed here.
- A5 [artifact] `docs/v3/tool-cost.md` measures every v2 command an agent or the lead runs (status, brief, check, verify, land, accept, job submit, job list, wait, quiet check) on this machine's real state: wall, CPU and syscalls per call, calls per day from the records, and where the time goes (profile). The lead measured `campaign status` at 11.4 s wall (10.5 s user, 9.8 s sys) and `brief` at 1.5 s on 2026-11-19 19h; the owner's target is milliseconds. It states what the state and record sizes imply for v3's language and storage (Python with an index, a compiled language, SQLite, an append-only log), with a prototype timing for the one hot path, not opinion.
- A6 [artifact] `docs/v3/leftovers.md` inventories what v2 leaves behind, by owner and age: disk under ~/.scratch (the lead measured 152 GiB, 115.6 GiB in the job store, 84.2 GiB of it build slots) and quest worktrees; jobs or quests in no terminal state that nothing will advance (zombies); worker children unreaped; duplicate work (the same command run at code that differs only outside its inputs; quests that redid landed fixes, such as loop-yield-tsan on 2026-11-19); with bytes, CPU minutes and tokens each cost.

## Contracts

None: analysis and docs only; v2 is not changed.

## Decisions

- Read-only over v2's records: never modify ~/.scratch/campaign, state.json, the worker or other campaigns' files.
- Simulation is a model; state its assumptions and calibration error next to every result.

## May land early

The inventory and the time accounting before the simulation.
