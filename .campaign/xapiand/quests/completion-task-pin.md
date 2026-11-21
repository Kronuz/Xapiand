# completion-task-pin: completion pins the task commit journal and consensus pin

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status completion-task-pin`.

## Outcome

completion pins task 18eb516 while journal and consensus pin task 95798a0 (four commits later:
blocking_scheduler, the settle-behind-cancel fix, notes). Detent's pin wave then meets two task
pins (Detent's lead, msg-193). completion moves to task 95798a0, so all three declare one task.
journal's and consensus's own completion pins move to this commit after the quests that own their
`CMakeLists.txt` land (journal-turn-buffers, node-admission).

## Acceptance criteria

- A1 completion builds and its suite passes with task 95798a0: the build-test job
- A2 [ci] CI is green on every platform

## Contracts

Consumes task at 95798a0; changes no completion API.

## May land early

Nothing: one landing.
