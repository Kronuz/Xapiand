# task-process-step: task: process step (run_daemon, ProcessLock, locked_pid)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-process-step`.

## Outcome

The note's (cpp-libraries:04a75c0 docs/server-composition.md) C7: Detent's `run_daemon`, `ProcessLock` and `locked_pid` (`detent_cli/daemon.hh:10-30`) move to task. Plan: docs/design/lifecycle-adoption.md quest 4 (about 400 lines). After task-driver-layout.

## Acceptance criteria

- A1 A detached child's `ready()` releases the parent with 0; a throw before it exits the parent with its status and message; the startup timeout exits nonzero: tests (fork)
- A2 A second lock on a held path fails; `locked_pid` names no process for a stale file: tests. Suite and CI

## Contracts

Provides: `run_daemon`, `ProcessLock`, `locked_pid` in task `process.hh`. Consumed by process-and-node, which replaces Xapiand's `detach` and `writepid`.

## Decisions

- D1 (library quests filed here, Detent D81), C7 of the note.
- Q4 (second-instance exit status) and Q5 (tools on a live directory) bind process-and-node, not this quest; the primitive only reports a held lock.

## May land early

None.
