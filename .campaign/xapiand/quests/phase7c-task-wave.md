# phase7c-task-wave: Add task-aware shutdown accounting

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:b6b8f7534 (2026-11-03); its ledger `docs/status/phase7c-task-wave.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-task-wave`.

## Outcome

Hosts forward scheduler ownership and root information, while counted coroutine frames track detached work. Shutdown cancels and drains chores, closes HTTP hosts, destroys roots, then closes indexes; published control-plane wakes are cleared safely.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] 1 `AsioHost` wraps `reactor(i).scheduler()`: next half (http wave): `docs/status/phase7c-task-wave.md`
- A2 [artifact] 2 `Host` and `Run` forward `roots`, `runs_here`, `post_call`; mutants M6, M7 die: `docs/status/phase7c-task-wave.md`
- A3 [artifact] 3 Four-phase teardown on C3; mutant M4 dies; HTTP service phases are next half: `docs/status/phase7c-task-wave.md`
- A4 [artifact] 4 Clear published wakes by scope guard; housekeeping stop stays first; mutants M1, M2 die: `docs/status/phase7c-task-wave.md`
- A5 [artifact] 5 `BlockingScheduler::run` owns no roots; detached tasks are counted by frame; mutant M3 dies: `docs/status/phase7c-task-wave.md`
- A6 [artifact] 6 Link `take_turn` deadline to request token: next half (F2): `docs/status/phase7c-task-wave.md`
- A7 [artifact] 7 Shutdown test checks frames return to baseline; service phases are next half: `docs/status/phase7c-task-wave.md`
- A8 [artifact] 8 Delete `Run::await`: next half (H1 removes caller): `docs/status/phase7c-task-wave.md`
- A9 [artifact] 9 Reactor Session migration: next half: `docs/status/phase7c-task-wave.md`
- A10 [artifact] Floor: separate ASan+UBSan and TSan Linux builds, macOS ASan, harnesses and 0 owed: `docs/status/phase7c-task-wave.md`
- A11 [artifact] g++-13 ctest hang stays at C3: `docs/status/phase7c-task-wave.md`
- A12 [artifact] Next-half items 1, 6, 8, 9 and HTTP phases were completed in `phase7c-http-wave`: `docs/status/phase7c-task-wave.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- `Host::post_call` pure virtual; `runs_here` and `roots` virtual; `BlockingScheduler` queues resumes and owns no roots.
- Count `Tasks` and registry detached tasks by coroutine frame; count chores until their task resumes.
- Shutdown order: cancel chores, close HTTP hosts, destroy roots; control loop phases run last.
- Risk high: Changes task cancellation, scheduler behavior, and teardown concurrency (the lead's mapping).

## May land early

None.
