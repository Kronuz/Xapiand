# http-log-logger-pin: http-log pins a logger without the scheduler

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status http-log-logger-pin`.

## Outcome

http-log's logger pin (f61d8df) predates logger:b121f2e, which dropped the scheduler, so a
standalone http-log build still fetches scheduler. Detent's lead asked for it (2026-11-21) so stash
and scheduler can be archived. The pin moves to logger 455cad0, the commit Xapiand pins.

## Acceptance criteria

- A1 http-log builds and its tests pass at the new pin: the `build-test` check
- A2 A standalone configure fetches no scheduler: a job at the head that configures into a fresh
  build directory and finds no `_deps/scheduler-src`
- A3 [ci] CI is green on every platform

## Contracts

None.

## May land early

Nothing: one landing.
