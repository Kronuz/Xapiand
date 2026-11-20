# manager-periodic: manager.cc's shutdown retry runs as a control::Periodic task

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status manager-periodic`.

## Outcome

The shutdown retry (try_shutdown every 5 s once an asap shutdown is requested) runs as a
control::Periodic task on the control loop, like housekeeping, so src/manager.cc no longer uses
reactor::PeriodicTimer. Detent's reactor-on-wheel (its D76) removes PeriodicTimer from reactor
once no Kronuz repository uses it.

## Criteria

- A1 The suite passes at the landed code, the shutdown tests included (gate check `suite`).
- A2 [artifact] `src/manager.cc` holds no PeriodicTimer: `grep -n PeriodicTimer src/manager.cc` prints nothing.
- A3 [manual] The lead read the diff: the task starts once however often shutdown_impl runs, and it stops with housekeeping at teardown.

## May land early

Nothing; it is one small change.
