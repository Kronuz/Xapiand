# task-process-step: journal

State: ready for review
Quest: .campaign/xapiand/quests/task-process-step.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done. Code branch quest/xapiand/task-process-step at task-wt/xapiand/task-process-step (d02ca88).

## Next

1. Review and land.

## Blocked on

Nothing.

## Evidence

Source: Detent src/detent_cli/daemon.cc and daemon.hh, last changed at Detent:276bc58d (Detent main c2f10b20, which contains 03eac9c5).

| Criterion | Evidence |
|---|---|
| A1 | pass job 261119-135236-15cba5 (all cases); suite 261119-135259-c55e35. Mutants (plain test, fails-without): ready never written 261119-135244-853d1c; message dropped 261119-135246-098fe2; no SIGTERM at timeout 261119-135249-e2e328 |
| A2 | same pass job and suite. Mutants: second lock acquires 261119-135252-fe16f4; stale file named 261119-135254-cd52b7 |

## Result

`process.hh` (header-only, namespace task, POSIX): `run_daemon`, `ProcessLock`, `locked_pid`, a faithful port; Ready is named `daemon_ready` and the helpers sit in `process_detail`. Outcomes are exposed (held lock: `acquired()` false; failed or timed-out startup: throw), no exit code chosen. Tests: test/process_test.cc (ready, detach, throws, failed, timeout, in-place, lock, locked-pid, unsafe). Doc: docs/mechanisms/process.md. Not run: CI (not pushed).

## Rulings I made

- AGENTS.md and docs/API.md entries re-added at e68d46f after the lead widened the file set; suite 261119-135206-202326 (reused: same tree as 1cc2223).
- Mutants lived on local branches (deleted); their commits are the ones named by the jobs.
